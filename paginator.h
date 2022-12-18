#pragma once

#include <vector>
#include <utility>
#include <iostream>

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator start, Iterator end, const size_t page_size) {
    
        while (start != end) {
            auto dist = distance(start, end);
            if (dist >= page_size) {
                pages_.push_back(make_pair(start, start + page_size));
                advance(start, page_size);
            } else {
                pages_.push_back(make_pair(start, end));
                start = end;
            }
        }
    }
    
    auto GetPages() const{
        return pages_;
    }
    
    auto begin() const{
        return pages_.begin();
    }
    
    auto end() const{
        return pages_.end();
    }
    
    size_t size() const{
        return pages_.size();
    }
    
private:
    std::vector<std::pair<Iterator, Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}



std::ostream &operator<<(std::ostream &os, const Document &doc) {
   {
   using namespace std;
      os << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
   }

    return os;
}

template <typename Iterator>
std::ostream &operator<<(std::ostream &os, const std::pair<Iterator, Iterator> &pages) {
    
    Iterator start = pages.first;
    Iterator end = pages.second;
    
    for (; start != end; ++start) {
        os << *start;
    }
    return os;
}