// #include "search_server.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

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

// Подставьте вашу реализацию класса SearchServer сюда
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

    // AddDocument - добавляет информацию о словах в документе в сервер
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

// шаблон вывода в поток элементов контейнера поочерёдно с запятыми
template <typename T>
void Print(ostream &out, const T &container)
{
    string firstt = ", "s;
    for (const auto &element : container)
    {
        if (!firstt.empty())
        {
            out << element;
            firstt = ""s;
        }
        else
        {
            out << ", "s << element;
        }
    }
    return;
}

// шаблонный оператор вывода в поток vector
template <typename VECTOR>
ostream &operator<<(ostream &out, const vector<VECTOR> &container)
{
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

// шаблонный оператор вывода в поток множества
template <typename SET>
ostream &operator<<(ostream &out, const set<SET> &container)
{
    out << "{";
    Print(out, container);
    out << "}"s;
    return out;
}

// шаблонный оператор вывода в поток пар
template <typename FIRST, typename SECOND>
ostream &operator<<(ostream &out, const pair<FIRST, SECOND> &container)
{
    out << container.first << ": "s << container.second;
    return out;
}

// шаблонный оператор вывода в поток словаря
template <typename KEY, typename VALUE>
ostream &operator<<(ostream &out, const map<KEY, VALUE> &container)
{
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint)
{
    if (t != u)
    {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty())
        {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint)
{
    if (!value)
    {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty())
        {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(Func func, const string &function)
{
    func();
    cerr << function << " OK"s;
    cerr << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

/* Разместите код остальных тестов здесь */
// добавляем документы
void TestAddDocuments()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        server.AddDocument(doc_id + 1, "big bird on window", DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 2);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

// удаляется документ с минус-словом
void TestExcludeMinusWords()
{
    const int doc_id = 42;
    const string content = "bat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    // совпадения по bat нет, т.к. city минус-слово
    ASSERT_HINT(server.FindTopDocuments("bat in the -city"s).empty(), "Minus-words must exclude document"s);

    // в данном случае city не минус-слово
    const auto found_docs = server.FindTopDocuments("cat in the city"s);
    ASSERT_HINT(found_docs.size() == 1, "There are no minus-words"s);
    const Document &doc0 = found_docs[0];
    ASSERT(doc0.id == doc_id);
}

// проверка матчинга документов
void TestMatchedDocuments()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [matched_words, status] = server.MatchDocument("cat in the city"s, doc_id);
        const vector<string> resultat = {"cat"s, "city"s};
        ASSERT_EQUAL_HINT(resultat, matched_words, "Words is not equal !!!");
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [matched_words, status] = server.MatchDocument("dog in the big -city"s, doc_id);
        // city - минус-слово обнуляет документ
        const vector<string> resultat = {};
        ASSERT_EQUAL(resultat, matched_words);
        ASSERT_HINT(matched_words.empty(), "Minus-words must exclude document"s);
    }
}

// Проверка сортировки релевантности по убыванию
void TestSortByRelevance()
{
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(43, "cat cat the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(44, "cat cat cat city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(45, "cat cat cat cat"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(46, "bat on my finger"s, DocumentStatus::ACTUAL, {1, 2, 3});

    vector<int> relevanc;
    for (const Document &document : server.FindTopDocuments("cat cat"s))
    {
        relevanc.push_back(document.id);
    }
    vector<int> tmp = {45, 44, 43, 42};
    ASSERT_EQUAL(relevanc, tmp);
}

// проверка вычисления рейтинга
void TestCalculateRating()
{
    {
        SearchServer server;
        const int doc_id = 42;
        const string content = "bird in the box box box"s;
        const vector<int> rating = {1, 2, 3};
        const int average_rtng = round((1 + 2 + 3 * 1.0) / 3.0);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, rating);
        const auto found_docs = server.FindTopDocuments("box bat snake"s);
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].rating, average_rtng);
    }
}

// проверка предиката
void TestSearchByPredicateLambda()
{
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "bat in the home"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(3, "big fox in the forest"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in the city"s, [](int document_id, DocumentStatus status, int rating)
                                                        { return rating > 0; });
        // прежде чем проверять элемент контейнера found_docs 
        // стоит проверить, что нужное количество там есть
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_HINT(found_docs[0].id == 1, "Predicate is wrong");
    }
}

// проверка статуса
void TestSearchByStatus()
{
    const int doc_id = 42;
    const string content = "cat in the house"s;
    const vector<int> ratings = {1, 2, 3};
    const DocumentStatus document_status = DocumentStatus::BANNED;

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat bat bird"s, DocumentStatus::BANNED);
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat bat bird"s, DocumentStatus::ACTUAL);
        ASSERT_HINT(found_docs.empty(), "Wrong status"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat bat bird"s, DocumentStatus::IRRELEVANT);
        ASSERT_HINT(found_docs.empty(), "Wrong status"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat bat bird"s, DocumentStatus::REMOVED);
        ASSERT_HINT(found_docs.empty(), "Wrong status"s);
    }
}

// проверка вычисления релевантности
void TestCalculateRelevance()
{
    {
        SearchServer server;
        const vector<int> rating = {1, 2, 3};
        server.AddDocument(42, "black bat with long hair"s, DocumentStatus::ACTUAL, rating);
        server.AddDocument(43, "nice bat nice bird"s, DocumentStatus::ACTUAL, rating);
        server.AddDocument(44, "cute fox big heart"s, DocumentStatus::ACTUAL, rating);
        const auto found_docs = server.FindTopDocuments("nice cute bat"s);
        double relevance_query = log((3 * 1.0) / 1) * (2.0 / 4.0) + log((3 * 1.0) / 2) * (1.0 / 4.0);
        ASSERT_HINT(abs(found_docs[0].relevance - relevance_query) < 1e-6, "Calculate relevance is wrong"s);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer()
{
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestExcludeMinusWords);
    RUN_TEST(TestMatchedDocuments);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestCalculateRating);
    RUN_TEST(TestSearchByPredicateLambda);
    RUN_TEST(TestSearchByStatus);
    RUN_TEST(TestCalculateRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main()
{
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}