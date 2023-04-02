#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <stdexcept>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text)
{
    vector<string> words;
    string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

struct Document
{
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id), relevance(relevance), rating(rating)
    {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer &strings)
{
    set<string> non_empty_strings;
    for (const string &str : strings)
    {
        if (!str.empty())
        {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer
{
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        if (any_of(stop_words_.begin(), stop_words_.end(), [](const auto &word)
                   { return !IsValidWord(word); }))
        {
            throw invalid_argument("Invalid stop_word");
        }
    }

    // Invoke delegating constructor from string container
    explicit SearchServer(const string &stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
    {
    }

    void AddDocument(int document_id, const string &document, DocumentStatus status,
                     const vector<int> &ratings)
    {
        if (document_id < 0 || documents_.count(document_id) || !IsValidWord(document) || !IsValidQuery(document))
        {
            throw invalid_argument("Error! Document contains wrong symbols or ID"s);
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        id_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string &raw_query, DocumentPredicate document_predicate) const
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs)
             {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON)
                 {
                     return lhs.rating > rhs.rating;
                 }
                 else
                 {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status) const
    {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating)
            { return document_status == status; });
    }

    vector<Document> FindTopDocuments(const string &raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const
    {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query, int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string &word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }
        for (const string &word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

    int GetDocumentId(int index) const
    {
        if (index < 0 || index > id_.size())
        {
            throw out_of_range("Error! Out of range"s);
        }
        return id_.at(index);
    }

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> id_;

    bool IsStopWord(const string &word) const
    {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string &word)
    {
        return none_of(word.begin(), word.end(), [](char c)
                       { return c >= '\0' && c < ' '; });
    }

    static bool IsValidQuery(const string &query)
    {
        if (query.empty())
        {
            return false;
        }
        // Проверяем, что запрос состоит из валидных слов, разделённых пробелами и минусами
        for (int i = 0; i < query.size(); ++i)
        {
            if (query[i] == '-' || query[i] == ' ')
            {
                continue;
            }
            string word;
            while (query[i] != '-' && query[i] != ' ' && i < query.size())
            {
                word += query[i];
                ++i;
            }
            if (!IsValidWord(word))
            {
                return false;
            }
        }
        // Проверяем, что запрос не состоит только из минус-слов
        size_t minus_words_count = count(query.begin(), query.end(), '-');
        size_t words_count = count_if(query.begin(), query.end(), [](char c)
                                      { return c != '-' && c != ' '; });
        if (minus_words_count == words_count)
        {
            return false;
        }
        return true;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const
    {
        vector<string> words;
        for (const string &word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int> &ratings)
    {
        if (ratings.empty())
        {
            return 0;
        }
        int rating_sum = 0;
        rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const string &text) const
    {
        if (text.empty())
        {
            throw invalid_argument("Error! Empty query word"s);
        }
        bool is_minus = false;
        string word = text;
        if (text[0] == '-')
        {
            is_minus = true;
            if (text.size() == 1)
            {
                throw invalid_argument("Error! Minus sign without query word"s);
            }
            word = text.substr(1);
        }
        if (!IsValidWord(word))
        {
            throw invalid_argument("Error! Invalid query word: "s + text);
        }
        bool is_stop_word = IsStopWord(text);
        return {text, is_minus, is_stop_word};
    }

    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string &text) const
    {
        Query query;
        for (string &word : SplitIntoWords(text))
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string &word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query &query,
                                      DocumentPredicate document_predicate) const
    {
        map<int, double> document_to_relevance;
        for (const string &word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                const auto &document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

void PrintDocument(const Document &document)
{
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}
int main()
{
    setlocale(LC_ALL, "Russian");
    SearchServer search_server("и в на"s);
    try
    {
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        cout << "OK" << endl;
        search_server.AddDocument(2, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
        cout << "OK" << endl;
        search_server.AddDocument(3, "\x20можноэто32 и далее тоже"s, DocumentStatus::ACTUAL, {1, 2});
        cout << "OK" << endl;
        search_server.AddDocument(4, "\x21 можнэто33 пёс скворец"s, DocumentStatus::ACTUAL, {1, 3, 2});
        cout << "OK" << endl;
        const auto documents = search_server.FindTopDocuments("\x19 нельзя это 31 пушистый"s);
        cout << "OK" << endl;
        for (const Document &document : documents)
        {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument &e)
    {
        cout << "Error: " << e.what() << endl;
    }
    catch (const out_of_range &e)
    {
        cout << "Error: " << e.what() << endl;
    }
}