/*
 * KBibTeX Standalone Search Library
 * A simplified literature search interface
 */

#ifndef BIBSEARCH_H
#define BIBSEARCH_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace BibSearch {

// Structure representing a bibliographic entry
struct BibEntry {
    std::string id;
    std::string type; // article, book, inproceedings, etc.
    std::map<std::string, std::string> fields;

    // Common field accessors
    std::string getTitle() const {
        auto it = fields.find("title");
        return (it != fields.end()) ? it->second : "";
    }

    std::string getAuthors() const {
        auto it = fields.find("author");
        return (it != fields.end()) ? it->second : "";
    }

    std::string getYear() const {
        auto it = fields.find("year");
        return (it != fields.end()) ? it->second : "";
    }

    std::string getDOI() const {
        auto it = fields.find("doi");
        return (it != fields.end()) ? it->second : "";
    }

    std::string getAbstract() const {
        auto it = fields.find("abstract");
        return (it != fields.end()) ? it->second : "";
    }

    std::string getURL() const {
        auto it = fields.find("url");
        return (it != fields.end()) ? it->second : "";
    }

    // Convert to BibTeX format
    std::string toBibTeX() const;

    // Convert to JSON format
    std::string toJSON() const;
};

// Search query parameters
struct SearchQuery {
    std::string author;
    std::string title;
    std::string keywords; // free text search
    std::string year;
    int maxResults = 10;
};

// Available search engines
enum class SearchEngine {
    PubMed,
    ArXiv,
    GoogleScholar,
    CrossRef,  // DOI
    IEEE,
    ACM,
    SemanticScholar,
    All  // Search all available engines
};

// Search result callback
using ResultCallback = std::function<void(const BibEntry&)>;
using ErrorCallback = std::function<void(const std::string&)>;
using ProgressCallback = std::function<void(int current, int total)>;

// Main search interface
class LiteratureSearch {
public:
    LiteratureSearch();
    ~LiteratureSearch();

    // Set callbacks
    void setResultCallback(ResultCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setProgressCallback(ProgressCallback callback);

    // Perform synchronous search (blocks until complete)
    std::vector<BibEntry> search(const SearchQuery& query, SearchEngine engine = SearchEngine::All);

    // Perform asynchronous search (returns immediately, results via callback)
    void searchAsync(const SearchQuery& query, SearchEngine engine = SearchEngine::All);

    // Cancel ongoing search
    void cancelSearch();

    // Get list of available search engines
    static std::vector<std::string> getAvailableEngines();

    // Convert engine enum to string
    static std::string engineToString(SearchEngine engine);
    static SearchEngine stringToEngine(const std::string& engine);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Utility functions
namespace Utils {
    // Convert vector of entries to JSON array
    std::string entriesToJSON(const std::vector<BibEntry>& entries);

    // Convert vector of entries to BibTeX string
    std::string entriesToBibTeX(const std::vector<BibEntry>& entries);

    // Parse BibTeX string to entries
    std::vector<BibEntry> parseBibTeX(const std::string& bibtex);
}

} // namespace BibSearch

#endif // BIBSEARCH_H