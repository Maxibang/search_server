#pragma once

#include <vector>
#include <deque>
#include "document.h"
#include "search_server.h"
#include <string>
#include <string_view>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_server_(search_server){
    }
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string_view raw_query, DocumentPredicate document_predicate) {
        auto query_search_result = search_server_.FindTopDocuments(raw_query, document_predicate);
        FixQuerySearchResult(query_search_result, raw_query);
        return query_search_result;
    }
    
    std::vector<Document> AddFindRequest(const std::string_view raw_query, DocumentStatus status);
    
    std::vector<Document> AddFindRequest(const std::string_view raw_query);
    
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::string_view query;
        std::vector<Document> search_result;
        bool is_empty;
    };
    
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int current_empty_result = 0;
    void FixQuerySearchResult(const std::vector<Document> &result_of_search, const std::string_view query);
};
