#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

// считывает строку
string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

// считывает целочисл. переменную
int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

// разбивает строку на слова
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

// структура для хранения идентификатора, релевантности и рейтинга документа
struct Document
{
    int id;
    double relevance;
    int rating;
};

// статусы
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
    // разбивает строку на множество стоп-слов
    void SetStopWords(const string &text)
    {
        for (const string &word : SplitIntoWords(text))
        {
            stop_words_.insert(word);
        }
    }

    // добавляет информацию о словах в документе в сервер
    void AddDocument(int document_id, const string &document, DocumentStatus status,
                     const vector<int> &ratings)
    {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words)
        {
            // добавляет релевантность по слову и (второму ключу) id документа
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    // возвращает вектор документов в порядке убывания релевантности
    template <typename Predicate>
    vector<Document> FindTopDocuments(const string &raw_query, Predicate predicate) const
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs)
             {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6)
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

    // перегрузили метод для одного аргумента (второй по-умолчанию для статуса ACTUAL)
    // (FindTopDocuments без второго аргумента должен осуществлять поиск только по актуальным документам)
    vector<Document> FindTopDocuments(const string &raw_query) const
    {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating)
                                { return status == DocumentStatus::ACTUAL; });
    }

    // перегрузили метод для двух аргументов (вторым передаётся статус)
    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status) const
    {
        return FindTopDocuments(raw_query, [&status](int document_id, DocumentStatus status_upd, int rating)
                                { return status_upd == status; });
    }

    // возвращает количество документов в поисковой системе
    int GetDocumentCount() const
    {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                        int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        // итерируемся по множеству плюс-слов (слова УЖЕ упорядочены и не повторяются)
        for (const string &word : query.plus_words)
        {
            // если очередного плюс-слова нет в документе, то переходим к след. плюс-слову
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            // если очередное плюс-слово есть в документе,
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                // добавляем слово в вектор
                matched_words.push_back(word);
            }
        }
        // итерируемся по множеству минус-слов
        for (const string &word : query.minus_words)
        {
            // если очередного минус-слова нет в документе, то переходим к след. минус-слову
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            // если очередное минус-слово есть в документе,
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                // то очищаем вектор
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    // структура внутренних данных о документе (рейтинг и статус)
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    // множество стоп-слов
    set<string> stop_words_;

    // сопоставляет каждому слову словарь «документ - TF» (слово в документе, id и релевантность)
    map<string, map<int, double>> word_to_document_freqs_;

    // хранение идентификатора и рейтинга, статуса
    map<int, DocumentData> documents_;

    bool IsStopWord(const string &word) const
    {
        return stop_words_.count(word) > 0;
    }

    // разбивает строку на вектор слов и выкидывает стоп-слова
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
        for (const int rating : ratings)
        {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    // проверяет на минус-слова
    QueryWord ParseQueryWord(string text) const
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    // содержит множество плюс-слов и множество минус-слов
    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    // заполняет структуру Query множеством плюс-слов и минус-слов
    Query ParseQuery(const string &text) const
    {
        Query query;
        for (const string &word : SplitIntoWords(text))
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

    // считает IDF (обратная частота документа)
    double ComputeWordInverseDocumentFreq(const string &word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    // по множиству плюс- и минус-слов запроса находит все документы с наличием релевантности и фильтрует по статусу
    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query &query, Predicate predicate) const
    {
        map<int, double> document_to_relevance;
        for (const string &word : query.plus_words)
        {
            // если очередного плюс-слова нет в документе, то переходим к след. плюс-слову
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            // если очередное плюс-слово есть в словаре, то вычисляем его частоту
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            // по ключу (слову) открываем значения его идентификатора и частоты
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                // по ключу ID сохраняем рейтинг и статус
                const auto &document_data = documents_.at(document_id);
                // если второй аргумент, принимаемый в виде лямбды в  FindTopDocuments
                // (ID, STATUS, RATING) вовращает true, то
                if (predicate(document_id, document_data.status, document_data.rating))
                {
                    // рассчитываем релевантность
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word : query.minus_words)
        {
            // если очередного минус-слова нет в словаре, то идём к след. минус-слову
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            // если есть минус-слово в документе, удаляем весь документ по его id
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        // вектор структур (идентификатор - релевантность)
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document &document)
{
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main()
{
    // создали объект класса
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    // вызвали метод AddDocument 4 раза и передали в качестве параметров ID, STATUS, rating
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    // вызвали метод FindTopDocuments с 1 параметром
    for (const Document &document : search_server.FindTopDocuments("пушистый ухоженный кот"s))
    {
        PrintDocument(document);
    }
    // вызвали метод FindTopDocuments с 2 параметрами (второй - статус документа)
    cout << "BANNED:"s << endl;
    for (const Document &document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED))
    {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    // вызвали метод FindTopDocuments с 2 параметрами
    // второй параметр - лямбда функция,
    // которая преобразуется в реализации метода в функциональный объект
    for (const Document &document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating)
                                                                   { return document_id % 2 == 0; }))
    {
        PrintDocument(document);
    }
    return 0;
}