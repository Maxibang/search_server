#include "search_server.h"
#include <numeric>

using namespace std;

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw std::invalid_argument("Invalid document_id"s);
	}
    
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, string(move(document))});
    
    const auto words = SplitIntoWordsNoStop(documents_.at(document_id).document_text);
	const double inv_word_count = 1.0 / words.size();
	
    for (const auto word : words) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		(docs_term_freqs_[document_id])[word] += 1;
	}
	document_ids_.push_back(document_id);
}
// #1
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
}

// #2
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
    
}

// #3
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus status) const noexcept {
    return FindTopDocuments(execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
} 

// #4
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
	return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

// #5
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}


// #6
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const noexcept{
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

int SearchServer::GetDocumentId(int index) const {
	return document_ids_.at(index);
}

bool SearchServer::IsStopWord(const string_view word) const {
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view word) {
	// A valid word must not contain special characters
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
	vector<string_view> words;
	for (const auto word : SplitIntoWordsView(text)) {
		if (!IsValidWord(word)) {
			throw invalid_argument("Word is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

vector<int>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}
vector<int>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
	if (docs_term_freqs_.count(document_id)) {
		return docs_term_freqs_.at(document_id);
	}
	else {
		return empty_map_;
	}
}

void SearchServer::RemoveDocument(int document_id) {
	if (documents_.count(document_id)) {
           const auto cnt_to_erase = docs_term_freqs_.at(document_id).size();
            std::vector<std::string_view> words_to_erase(cnt_to_erase);
        transform(std::execution::seq,
                  docs_term_freqs_.at(document_id).begin(), docs_term_freqs_.at(document_id).end(),
                  words_to_erase.begin(),
                  [](const auto& item) { return item.first; }
        );
        for_each(std::execution::seq,
                 words_to_erase.begin(), words_to_erase.end(),
                 [this, document_id](const auto &word) {word_to_document_freqs_.at(word).erase(document_id);});
        }
		documents_.erase(document_id);
		document_ids_.erase(find(document_ids_.begin(), document_ids_.end(), document_id));
		docs_term_freqs_.erase(document_id);

	}
       void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
           	if (documents_.count(document_id)) { 
           const auto cnt_to_erase = docs_term_freqs_.at(document_id).size();
            std::vector<std::string_view> words_to_erase(cnt_to_erase);
        transform(std::execution::par,
                  docs_term_freqs_.at(document_id).begin(), docs_term_freqs_.at(document_id).end(),
                  words_to_erase.begin(),
                  [](const auto& item) { return item.first; }
        );
        for_each(std::execution::par,
                 words_to_erase.begin(), words_to_erase.end(),
                 [this, document_id](const auto &word) {word_to_document_freqs_.at(word).erase(document_id);});
        }
		documents_.erase(document_id);
		document_ids_.erase(find(document_ids_.begin(), document_ids_.end(), document_id));
		docs_term_freqs_.erase(document_id);
       }
    
    
void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    RemoveDocument(document_id);
}


// TASK 2/3 Sprint 9 Parallel MatchDocument

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view text) const {
	if (text.empty()) {
		throw invalid_argument("Query word is empty"s);
	}
	string_view word = text;
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw invalid_argument("Query word is invalid");
	}

	return { word, is_minus, IsStopWord(word) };
}


SearchServer::Query SearchServer::ParseQuery(bool flag, const string_view text) const {
	Query result;
	for (const auto word : SplitIntoWordsView(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.push_back(query_word.data);
			}
			else {
				result.plus_words.push_back(query_word.data);
			}
		}
	}
    
    if (flag) {
        sort(result.minus_words.begin(), result.minus_words.end());
        auto it1 = unique(execution::par, result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(it1, result.minus_words.end());
        sort(execution::par, result.plus_words.begin(), result.plus_words.end());
        auto it2 = unique(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(it2, result.plus_words.end());        
    }
    
	return result;
}


tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view &raw_query, int document_id) const {
	const auto query = ParseQuery(true, raw_query);
	vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    
    /* WHY SOLUTION FROM 2/3 doesn't pass the test here with message ????
    "method without explicit execution policy is too slow, student/author ratio: 1.66182802507"
    How it works????? In previous step this solution is valid, but here by transition from string to string_view it becomes slower and invalid by meaning of cheking system!!!
    
	for (const auto &word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			matched_words.push_back(word);
		}
	}
	for (const auto &word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			matched_words.clear();
			break;
		}
	}
	return { matched_words, documents_.at(document_id).status };
    */
    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), [&, document_id](const auto &word){if(word_to_document_freqs_.count(word) > 0) {return word_to_document_freqs_.at(word).count(document_id) > 0;} return false;})) {
       return {vector<string_view>{}, documents_.at(document_id).status}; 
    }
    
    auto it = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&, document_id](const auto &word){ if(word_to_document_freqs_.count(word) > 0) {return word_to_document_freqs_.at(word).count(document_id) > 0;} return false; });
    
    sort(execution::par, matched_words.begin(), it);
    auto last = unique(execution::par, matched_words.begin(), it);
    matched_words.erase(last, matched_words.end());
    return { matched_words, documents_.at(document_id).status };
    
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, const std::string_view &raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const {
	
    if (documents_.count(document_id) == 0) {
        throw std::out_of_range("Invalid document_id: such document_id isn't exists"s);
    }
    
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("Invalid query");
    }  
    
    
	const auto query = ParseQuery(false, raw_query);
    
    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), [&, document_id](const auto &word){if(word_to_document_freqs_.count(word) > 0) {return word_to_document_freqs_.at(word).count(document_id) > 0;} return false;})) {
       return {vector<string_view>{}, documents_.at(document_id).status}; 
    }
    
	vector<string_view> matched_words(query.plus_words.size());
    auto it = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&, document_id](const auto &word){ if(word_to_document_freqs_.count(word) > 0) {return word_to_document_freqs_.at(word).count(document_id) > 0;} return false; });
    
    sort(execution::par, matched_words.begin(), it);
    auto last = unique(execution::par, matched_words.begin(), it);
    matched_words.erase(last, matched_words.end());
    return { matched_words, documents_.at(document_id).status };
}