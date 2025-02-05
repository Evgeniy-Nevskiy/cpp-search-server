#pragma once
#include "search_server.h"
#include <deque>

class RequestQueue
{
public:
    RequestQueue(const SearchServer &search_server) : search_request(search_server) {}

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(std::string_view raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult
    {
        bool query_result;
    };

    const SearchServer &search_request;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(std::string_view raw_query,
                                                   DocumentPredicate document_predicate)
{
    std::vector<Document> temporary = search_request.FindTopDocuments(raw_query,
                                                                   document_predicate);
    QueryResult query;
    query.query_result = (temporary.empty() == false);

    if (!(requests_.size() < min_in_day_))
    {
        requests_.pop_front();
    }

    requests_.push_back(query);
    return temporary;
}