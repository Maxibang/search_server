#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <set>
#include <map>
#include <algorithm>
#include "document.h"
#include "string_processing.h"
#include <stdexcept>
#include <iterator>
#include <execution>
#include <string_view>
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double ACCURACY = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }

    explicit SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWordsView(stop_words_text)) {}
    explicit SearchServer(const std::string_view stop_words_text) : SearchServer(SplitIntoWordsView(stop_words_text)) {}

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    
    // Previous ordinary FindTopDocuments without execution policy
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
        
        const auto query = ParseQuery(true, raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < ACCURACY) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }
    
    
    // Take an execution policy
    template <typename DocumentPredicate, typename ExecPolicy>
    std::vector<Document> FindTopDocuments(ExecPolicy policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        
        const auto query = ParseQuery(true, raw_query);
        auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

        sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < ACCURACY) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }
    
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus status) const noexcept;
    
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const noexcept;
    
    
    int GetDocumentCount() const;
    int GetDocumentId(int index) const;


    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    std::vector<int>::const_iterator begin() const;
    std::vector<int>::const_iterator end() const;
    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);   
    
    // TASK 2/3 Sprint 9 Parallel MatchDocument
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view &raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view &raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string document_text;
    };
    const std::set<std::string_view> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;
    
    std::map<int, std::map<std::string_view, double>> docs_term_freqs_;
    std::map<std::string_view, double> empty_map_;


    bool IsStopWord(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(bool flag, const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    //Sequenced policy FindAllDocuments
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for (const auto word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const auto word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

    
// Parallel policy FindAllDocuments
    template <typename DocumentPredicate, typename ExecPolicy>
    std::vector<Document> FindAllDocuments(ExecPolicy policy, const Query& query, DocumentPredicate document_predicate) const noexcept {
        ConcurrentMap<int, double> document_to_relevance(8);
        //std::map<int, double> document_to_relevance;
        
        std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&](const auto &word) {
            if (word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                } 
            }            
        });
        
        std::map<int, double> document_to_relevance_ = document_to_relevance.BuildOrdinaryMap();
        std::for_each(query.minus_words.begin(), query.minus_words.end(), [&](const auto &word){
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance_.erase(document_id);
                }  
            }
        });
        
        
        
        std::vector<Document> matched_documents(document_to_relevance_.size());
        for (const auto [document_id, relevance] : document_to_relevance_) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }   
};

