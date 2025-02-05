#include <execution>
#include <algorithm>
#include <list>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer &search_server,
                                                  const std::vector<std::string> &queries)
{
    std::vector<std::vector<Document>> temporary(queries.size());
    std::transform(std::execution::par,
                   queries.begin(),
                   queries.end(),
                   temporary.begin(),
                   [&search_server](auto query)
                   {
                       return search_server.FindTopDocuments(query);
                   });
    return temporary;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer &search_server,
                                           const std::vector<std::string> &queries)
{
    return std::transform_reduce(
        std::execution::par,
        queries.begin(),
        queries.end(),
        std::vector<Document>{},
        [](std::vector<Document> lhs, std::vector<Document> const &rhs)
        {
            lhs.insert(lhs.end(), rhs.begin(), rhs.end());
            return lhs;
        },
        [&search_server](auto query)
        {
            return search_server.FindTopDocuments(query);
        });
}