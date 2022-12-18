#include "request_queue.h"

using namespace std;

vector<Document> RequestQueue::AddFindRequest(const string_view raw_query, DocumentStatus status) {
    auto query_search_result = search_server_.FindTopDocuments(raw_query, status);
    FixQuerySearchResult(query_search_result, raw_query);
    return query_search_result;
}
    
vector<Document> RequestQueue::AddFindRequest(const string_view raw_query) {
    auto query_search_result = search_server_.FindTopDocuments(raw_query);
    FixQuerySearchResult(query_search_result, raw_query);
    return query_search_result;
}
    
int RequestQueue::GetNoResultRequests() const {
    return current_empty_result;
}
    
void RequestQueue::FixQuerySearchResult(const vector<Document> &result_of_search, const string_view query) {
    bool is_emty = result_of_search.empty();
    if (is_emty) {
        ++current_empty_result;
    }
        
    requests_.push_back({query, result_of_search, is_emty});
    if (requests_.size() >  1440) {
        if (requests_.front().is_empty == true) {
            --current_empty_result;
        }
        requests_.pop_front();
    }
} 