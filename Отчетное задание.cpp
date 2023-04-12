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

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
// 2. начало класса, внутри которого есть приватные и публичные поля. и все основные функции. пойдем по порядку.
class SearchServer {
public:
    void SetStopWords(const string& text) {                    //3. принимает список стоп-слов, и вызывает функцию, которая как раз парсит строку на отдельные слова. В принципе названия функций отражают их
        for (const string& word : SplitIntoWords(text)) {      // назначение. SplitIntoWords вернет нам вектор из слов, которые циклом складываются в вектор со стоп-словами.
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, // 4.1 Эта функция принимает документы и:
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);          // 4.2 убирает из запроса стоп-слова,
        const double inv_word_count = 1.0 / words.size();                          // 4.3 считает частоту упоминания каждого слова во всем тексте документа(TF). Параметр  Term Frequency пригодится для подсчета релевантности.
        for (const string& word : words) {                                         // 4.4 собирает нам хитрый словарь типа map<string, map<int, double>>, где ключ - это слово, а значение - еще одна мапа, в которой внутренний ключ id документа, значение - (TF);
            word_to_document_freqs_[word][document_id] += inv_word_count;          // 4.5 последние значение double для того, что бы правильней считать релевантность и правильно отсортировать результаты поиска.
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status}); // 4.5 заполняет структуру с полями, соответствующими значениями.
    }

    template<typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter filter_id_stat_rat // 5.1 На самом деле, этот метод вызывается в самом конце, но по нему можно почти весь класс и описать.
            /*DocumentStatus status = DocumentStatus::ACTUAL*/) const {                  // 5.2 метод принимает строку запроса, а в Filter принимается лямбда-функция, которая рассказывает как этому методу работать со статусами документов.
        const Query query = ParseQuery(raw_query);                                  // Это которые ACTUAL, BANNED, IRRELEVANT, REMOVED.
        auto matched_documents = FindAllDocuments(query, filter_id_stat_rat);            // 5.3 Парсим запрос и отправляем в самый большой метод, который ищет все совпадающие документы и возвращает релевантные. Думаю нет смысла описывать FindAllDocuments,
                                                                                         // он правда слишком большой. Скажу лишь, что в запросе могут быть слова с минусом "-хвост" и документ даже полностью совпавший с запросов, но имеющий слово "хвост", в результате не появится.
        sort(matched_documents.begin(), matched_documents.end(),                         // 5.4 Дальше, FindAllDocuments вернет вектор с типом структуры, в которой буду храниться все данные о совпавших по поиску документах. Этот метод рассчитает до конца релевантность и прочее.
             [](const Document& lhs, const Document& rhs) {                              // 5.5 потом нужно отсортировать результаты и метод sort это сделает, но его нужно научить, по какому параметру сортировать. Поэтому первыми параметрами задается диапазон сортировки,
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {                     // а третьим параметром передаётся лямбда-функция, которая научит его сортировать правильно.
                     return lhs.rating > rhs.rating;                                     // в условии первого if странная запись, но так как мы сортируем по рейтингу, а его тип double, то для более правильного сравнения применяется такой способ, так как он позволяет
                 } else {                                                                // сравнить числа, которые отличаются до шестого знака после запятой.
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {                    // 6.1 Далее нам, при большом количестве документов все результаты не понадобятся. Хватит первых пяти с наибольшей релевантностью и это здесь и происходит.
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);                       // 6.2 возвращаем все это в main и печатаем специальной функцией. Если вкратце, то как-то так=)
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

    vector<Document> FindTopDocuments (const string raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus documentStatus, int rating){
            return documentStatus == status;
        });
    }


private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Filter>
    vector<Document> FindAllDocuments(const Query& query, Filter filter_id_stat_rat) const {          //
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (filter_id_stat_rat(document_id,documents_.at(document_id).status,documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};



void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}
// 1. и так, все начинается здесь. в первой строке создаётся объект search_server, а затем вызываются методы.
// SetStopWords принимает список стоп-слов, которые будут игнорироваться при поиске.
// AddDocument принимает id, сам документ, его статус и рэйтинг. В прошлой версии кода, id присваивался автоматически.
// формируется запрос со статусом и разными формами дополнительных атрибутов, с которыми мы хотим поискать.
// затем вызывается функция, которая напечатает нам результат в консоль. Далее пойду по методам.
int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}
