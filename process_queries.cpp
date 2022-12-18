#include <vector>
#include <string>
#include <algorithm>
#include <execution>
#include "search_server.h"
#include "document.h"
#include <iostream>

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string> &queries) {
    std::vector<std::vector<Document>> result_vector(queries.size());
    std::transform(std::execution::par, queries.begin(), 
                   queries.end(), 
                   result_vector.begin(), 
                   [&search_server](const auto &query){return search_server.FindTopDocuments(query);});
    
    return result_vector;
}


std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string> &queries) {
    auto vec = ProcessQueries(search_server, queries);
    std::vector<Document> res_vec;
    res_vec.reserve(sizeof(vec));
    for (const auto &item : vec) {
        std::move(item.begin(), item.end(), std::back_inserter(res_vec));
    }
    //std::cout <<  res_vec.size() << " = size"s << endl;
    //std::cout <<  res_vec.capacity() << " = capacity"s << endl;
    return res_vec;
}