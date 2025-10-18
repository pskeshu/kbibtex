/*
 * KBibTeX Standalone Search Library
 * Core implementation
 */

#include "bibsearch.h"
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <curl/curl.h>
#include <json/json.h>
#include <regex>
#include <iomanip>

namespace BibSearch {

// Helper function for CURL callbacks
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// URL encoding helper
static std::string urlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

// BibEntry implementation
std::string BibEntry::toBibTeX() const {
    std::ostringstream oss;
    oss << "@" << type << "{" << id << ",\n";
    for (const auto& [key, value] : fields) {
        oss << "    " << key << " = {" << value << "},\n";
    }
    oss << "}\n";
    return oss.str();
}

std::string BibEntry::toJSON() const {
    Json::Value root;
    root["id"] = id;
    root["type"] = type;
    for (const auto& [key, value] : fields) {
        root["fields"][key] = value;
    }
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

// Implementation class
class LiteratureSearch::Impl {
public:
    ResultCallback resultCallback;
    ErrorCallback errorCallback;
    ProgressCallback progressCallback;
    bool cancelFlag = false;
    std::mutex mutex;

    // Helper function to filter entries by year filter (single year or range)
    bool matchesYearFilter(const BibEntry& entry, const SearchQuery& query) {
        // If no year filter at all, accept everything
        if (!query.hasYearRange() && query.year.empty()) {
            return true;
        }

        auto it = entry.fields.find("year");
        if (it == entry.fields.end()) {
            return false; // Reject if year filter is set but paper has no year
        }

        try {
            int entryYear = std::stoi(it->second);

            // Check year range if specified
            if (query.hasYearRange()) {
                int fromYear = std::stoi(query.yearFrom);
                int toYear = std::stoi(query.yearTo);
                return entryYear >= fromYear && entryYear <= toYear;
            }

            // Check single year if specified
            if (!query.year.empty()) {
                int queryYear = std::stoi(query.year);
                return entryYear == queryYear;
            }

            return true;
        } catch (...) {
            return false; // Reject if year parsing fails when filter is active
        }
    }

    std::string performHTTPGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "KBibTeX-Search/1.0");

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK && errorCallback) {
                errorCallback("CURL error: " + std::string(curl_easy_strerror(res)));
            }
            curl_easy_cleanup(curl);
        }
        return response;
    }

    std::vector<BibEntry> searchPubMed(const SearchQuery& query) {
        std::vector<BibEntry> results;

        // Build search query
        std::string searchTerm;
        if (!query.author.empty()) {
            searchTerm += query.author + "[Author]";
        }
        if (!query.title.empty()) {
            if (!searchTerm.empty()) searchTerm += "+AND+";
            searchTerm += query.title + "[Title]";
        }
        if (!query.keywords.empty()) {
            if (!searchTerm.empty()) searchTerm += "+AND+";
            searchTerm += query.keywords + "[All Fields]";
        }
        // Handle year or year range
        if (query.hasYearRange()) {
            if (!searchTerm.empty()) searchTerm += "+AND+";
            searchTerm += query.yearFrom + ":" + query.yearTo + "[pdat]";
        } else if (!query.year.empty()) {
            if (!searchTerm.empty()) searchTerm += "+AND+";
            searchTerm += query.year + "[pdat]";
        }

        // URL encode the search term
        searchTerm = urlEncode(searchTerm);

        // First, search for IDs
        std::string searchUrl = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi?db=pubmed&retmode=json&retmax="
                               + std::to_string(query.maxResults) + "&term=" + searchTerm;

        std::string searchResponse = performHTTPGet(searchUrl);

        // Parse JSON response to get IDs
        Json::Value searchJson;
        Json::Reader reader;
        if (!reader.parse(searchResponse, searchJson)) {
            if (errorCallback) errorCallback("Failed to parse PubMed search response");
            return results;
        }

        Json::Value idList = searchJson["esearchresult"]["idlist"];
        if (!idList.isArray() || idList.size() == 0) {
            return results; // No results found
        }

        // Build comma-separated list of IDs
        std::string ids;
        for (const auto& id : idList) {
            if (!ids.empty()) ids += ",";
            ids += id.asString();
        }

        // Fetch details for all IDs
        std::string fetchUrl = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=pubmed&retmode=xml&id=" + ids;
        std::string fetchResponse = performHTTPGet(fetchUrl);

        // Parse XML response (simplified - in real implementation would use proper XML parser)
        results = parsePubMedXML(fetchResponse);

        return results;
    }

    std::vector<BibEntry> parsePubMedXML(const std::string& xml) {
        std::vector<BibEntry> entries;

        // Simplified XML parsing using regex (for demonstration)
        // In production, use a proper XML parser like pugixml or tinyxml2
        // Using [\s\S] to match any character including newlines
        std::regex articleRegex("<PubmedArticle>([\\s\\S]*?)</PubmedArticle>");
        std::smatch articleMatch;
        std::string::const_iterator searchStart(xml.cbegin());

        while (std::regex_search(searchStart, xml.cend(), articleMatch, articleRegex)) {
            std::string article = articleMatch[1];
            BibEntry entry;

            // Extract PMID
            std::regex pmidRegex("<PMID[^>]*>(\\d+)</PMID>");
            std::smatch pmidMatch;
            if (std::regex_search(article, pmidMatch, pmidRegex)) {
                entry.id = "pmid" + pmidMatch[1].str();
            }

            // Extract title
            std::regex titleRegex("<ArticleTitle>(.*?)</ArticleTitle>");
            std::smatch titleMatch;
            if (std::regex_search(article, titleMatch, titleRegex)) {
                entry.fields["title"] = titleMatch[1];
            }

            // Extract authors (simplified)
            std::regex authorRegex("<LastName>(.*?)</LastName>.*?<ForeName>(.*?)</ForeName>");
            std::string authors;
            std::string::const_iterator authorStart(article.cbegin());
            while (std::regex_search(authorStart, article.cend(), titleMatch, authorRegex)) {
                if (!authors.empty()) authors += " and ";
                authors += titleMatch[2].str() + " " + titleMatch[1].str();
                authorStart = titleMatch.suffix().first;
            }
            if (!authors.empty()) {
                entry.fields["author"] = authors;
            }

            // Extract year
            std::regex yearRegex("<PubDate>.*?<Year>(\\d{4})</Year>");
            std::smatch yearMatch;
            if (std::regex_search(article, yearMatch, yearRegex)) {
                entry.fields["year"] = yearMatch[1];
            }

            // Extract journal
            std::regex journalRegex("<Title>(.*?)</Title>");
            std::smatch journalMatch;
            if (std::regex_search(article, journalMatch, journalRegex)) {
                entry.fields["journal"] = journalMatch[1];
            }

            // Extract abstract
            std::regex abstractRegex("<AbstractText[^>]*>(.*?)</AbstractText>");
            std::smatch abstractMatch;
            if (std::regex_search(article, abstractMatch, abstractRegex)) {
                entry.fields["abstract"] = abstractMatch[1];
            }

            // Extract DOI if available
            std::regex doiRegex("<ArticleId IdType=\"doi\">(.*?)</ArticleId>");
            std::smatch doiMatch;
            if (std::regex_search(article, doiMatch, doiRegex)) {
                entry.fields["doi"] = doiMatch[1];
            }

            entry.type = "article";
            entries.push_back(entry);

            searchStart = articleMatch.suffix().first;
        }

        return entries;
    }

    std::vector<BibEntry> searchArXiv(const SearchQuery& query) {
        std::vector<BibEntry> results;

        // Build arXiv search query
        std::string searchQuery;
        if (!query.author.empty()) {
            searchQuery += "au:" + urlEncode(query.author);
        }
        if (!query.title.empty()) {
            if (!searchQuery.empty()) searchQuery += "+AND+";
            searchQuery += "ti:" + urlEncode(query.title);
        }
        if (!query.keywords.empty()) {
            if (!searchQuery.empty()) searchQuery += "+AND+";
            searchQuery += "all:" + urlEncode(query.keywords);
        }

        std::string url = "http://export.arxiv.org/api/query?search_query=" + searchQuery
                         + "&max_results=" + std::to_string(query.maxResults);

        std::string response = performHTTPGet(url);

        // Parse Atom/XML feed (simplified)
        std::vector<BibEntry> allResults = parseArXivXML(response);

        // Filter by year range if specified
        if (query.hasYearRange()) {
            for (const auto& entry : allResults) {
                if (matchesYearFilter(entry, query)) {
                    results.push_back(entry);
                }
            }
        } else {
            results = allResults;
        }

        return results;
    }

    std::vector<BibEntry> parseArXivXML(const std::string& xml) {
        std::vector<BibEntry> entries;

        // Simplified XML parsing
        // Using [\s\S] to match any character including newlines
        std::regex entryRegex("<entry>([\\s\\S]*?)</entry>");
        std::smatch entryMatch;
        std::string::const_iterator searchStart(xml.cbegin());

        while (std::regex_search(searchStart, xml.cend(), entryMatch, entryRegex)) {
            std::string entryXml = entryMatch[1];
            BibEntry entry;

            // Extract ID
            std::regex idRegex("<id>http://arxiv.org/abs/(.*?)</id>");
            std::smatch idMatch;
            if (std::regex_search(entryXml, idMatch, idRegex)) {
                entry.id = "arxiv" + idMatch[1].str();
                entry.fields["eprint"] = idMatch[1];
                entry.fields["archivePrefix"] = "arXiv";
            }

            // Extract title
            std::regex titleRegex("<title>(.*?)</title>");
            std::smatch titleMatch;
            if (std::regex_search(entryXml, titleMatch, titleRegex)) {
                std::string title = titleMatch[1];
                // Remove newlines and extra spaces
                title = std::regex_replace(title, std::regex("\\s+"), " ");
                entry.fields["title"] = title;
            }

            // Extract authors
            std::regex authorRegex("<name>(.*?)</name>");
            std::string authors;
            std::string::const_iterator authorStart(entryXml.cbegin());
            std::smatch authorMatch;
            while (std::regex_search(authorStart, entryXml.cend(), authorMatch, authorRegex)) {
                if (!authors.empty()) authors += " and ";
                authors += authorMatch[1];
                authorStart = authorMatch.suffix().first;
            }
            if (!authors.empty()) {
                entry.fields["author"] = authors;
            }

            // Extract summary/abstract
            std::regex summaryRegex("<summary>([\\s\\S]*?)</summary>");
            std::smatch summaryMatch;
            if (std::regex_search(entryXml, summaryMatch, summaryRegex)) {
                std::string abstract = summaryMatch[1];
                abstract = std::regex_replace(abstract, std::regex("\\s+"), " ");
                entry.fields["abstract"] = abstract;
            }

            // Extract published date
            std::regex dateRegex("<published>(\\d{4})-");
            std::smatch dateMatch;
            if (std::regex_search(entryXml, dateMatch, dateRegex)) {
                entry.fields["year"] = dateMatch[1];
            }

            entry.type = "article";
            entries.push_back(entry);

            searchStart = entryMatch.suffix().first;
        }

        return entries;
    }

    std::vector<BibEntry> searchCrossRef(const SearchQuery& query) {
        std::vector<BibEntry> results;

        // Build query string
        std::string queryStr;
        if (!query.title.empty()) queryStr += query.title + " ";
        if (!query.author.empty()) queryStr += query.author + " ";
        if (!query.keywords.empty()) queryStr += query.keywords;

        queryStr = urlEncode(queryStr);

        std::string url = "https://api.crossref.org/works?query=" + queryStr
                         + "&rows=" + std::to_string(query.maxResults);

        std::string response = performHTTPGet(url);

        // Parse JSON response
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            if (errorCallback) errorCallback("Failed to parse CrossRef response");
            return results;
        }

        Json::Value items = root["message"]["items"];
        for (const auto& item : items) {
            BibEntry entry;

            // Extract DOI
            if (item.isMember("DOI")) {
                entry.fields["doi"] = item["DOI"].asString();
                entry.id = "doi" + std::regex_replace(item["DOI"].asString(), std::regex("/"), "_");
            }

            // Extract title
            if (item.isMember("title") && item["title"].isArray() && item["title"].size() > 0) {
                entry.fields["title"] = item["title"][0].asString();
            }

            // Extract authors
            if (item.isMember("author") && item["author"].isArray()) {
                std::string authors;
                for (const auto& author : item["author"]) {
                    if (!authors.empty()) authors += " and ";
                    if (author.isMember("given") && author.isMember("family")) {
                        authors += author["given"].asString() + " " + author["family"].asString();
                    }
                }
                if (!authors.empty()) {
                    entry.fields["author"] = authors;
                }
            }

            // Extract year
            if (item.isMember("published-print") && item["published-print"].isMember("date-parts")) {
                auto dateParts = item["published-print"]["date-parts"];
                if (dateParts.isArray() && dateParts.size() > 0 && dateParts[0].isArray()) {
                    entry.fields["year"] = std::to_string(dateParts[0][0].asInt());
                }
            }

            // Extract journal
            if (item.isMember("container-title") && item["container-title"].isArray() && item["container-title"].size() > 0) {
                entry.fields["journal"] = item["container-title"][0].asString();
            }

            // Extract volume and pages
            if (item.isMember("volume")) {
                entry.fields["volume"] = item["volume"].asString();
            }
            if (item.isMember("page")) {
                entry.fields["pages"] = item["page"].asString();
            }

            // Determine type
            if (item.isMember("type")) {
                std::string type = item["type"].asString();
                if (type == "journal-article") entry.type = "article";
                else if (type == "book") entry.type = "book";
                else if (type == "proceedings-article") entry.type = "inproceedings";
                else entry.type = "misc";
            } else {
                entry.type = "article";
            }

            // Filter by year range if specified
            if (query.hasYearRange()) {
                if (matchesYearFilter(entry, query)) {
                    results.push_back(entry);
                }
            } else {
                results.push_back(entry);
            }
        }

        return results;
    }

    std::vector<BibEntry> searchSemanticScholar(const SearchQuery& query) {
        std::vector<BibEntry> results;

        // Build query string for Semantic Scholar API
        std::string queryStr;
        if (!query.title.empty()) queryStr += query.title + " ";
        if (!query.author.empty()) queryStr += query.author + " ";
        if (!query.keywords.empty()) queryStr += query.keywords;

        if (queryStr.empty()) return results;

        queryStr = urlEncode(queryStr);

        // Semantic Scholar API v1 - no authentication required for basic searches
        std::string url = "https://api.semanticscholar.org/graph/v1/paper/search?query=" + queryStr
                         + "&limit=" + std::to_string(query.maxResults)
                         + "&fields=paperId,title,authors,year,abstract,url,citationCount,venue,publicationTypes";

        std::string response = performHTTPGet(url);

        // Parse JSON response
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            if (errorCallback) errorCallback("Failed to parse Semantic Scholar response");
            return results;
        }

        Json::Value papers = root["data"];
        if (!papers.isArray()) return results;

        for (const auto& paper : papers) {
            BibEntry entry;

            // Extract paper ID
            if (paper.isMember("paperId")) {
                entry.id = "s2_" + paper["paperId"].asString();
            }

            // Extract title
            if (paper.isMember("title")) {
                entry.fields["title"] = paper["title"].asString();
            }

            // Extract authors
            if (paper.isMember("authors") && paper["authors"].isArray()) {
                std::string authors;
                for (const auto& author : paper["authors"]) {
                    if (author.isMember("name")) {
                        if (!authors.empty()) authors += " and ";
                        authors += author["name"].asString();
                    }
                }
                if (!authors.empty()) {
                    entry.fields["author"] = authors;
                }
            }

            // Extract year
            if (paper.isMember("year") && !paper["year"].isNull()) {
                entry.fields["year"] = std::to_string(paper["year"].asInt());
            }

            // Extract abstract
            if (paper.isMember("abstract") && !paper["abstract"].isNull()) {
                entry.fields["abstract"] = paper["abstract"].asString();
            }

            // Extract URL
            if (paper.isMember("url") && !paper["url"].isNull()) {
                entry.fields["url"] = paper["url"].asString();
            }

            // Extract venue (journal/conference)
            if (paper.isMember("venue") && !paper["venue"].isNull()) {
                std::string venue = paper["venue"].asString();
                if (!venue.empty()) {
                    entry.fields["journal"] = venue;
                }
            }

            // Extract citation count (as a note)
            if (paper.isMember("citationCount") && !paper["citationCount"].isNull()) {
                entry.fields["note"] = "Cited by " + std::to_string(paper["citationCount"].asInt());
            }

            // Determine entry type
            entry.type = "article";
            if (paper.isMember("publicationTypes") && paper["publicationTypes"].isArray()) {
                for (const auto& pubType : paper["publicationTypes"]) {
                    std::string type = pubType.asString();
                    if (type == "Conference") {
                        entry.type = "inproceedings";
                        break;
                    } else if (type == "Book") {
                        entry.type = "book";
                        break;
                    }
                }
            }

            // Filter by year range if specified
            if (query.hasYearRange()) {
                if (matchesYearFilter(entry, query)) {
                    results.push_back(entry);
                }
            } else {
                results.push_back(entry);
            }
        }

        return results;
    }
};

// LiteratureSearch implementation
LiteratureSearch::LiteratureSearch() : pImpl(std::make_unique<Impl>()) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LiteratureSearch::~LiteratureSearch() {
    curl_global_cleanup();
}

void LiteratureSearch::setResultCallback(ResultCallback callback) {
    pImpl->resultCallback = callback;
}

void LiteratureSearch::setErrorCallback(ErrorCallback callback) {
    pImpl->errorCallback = callback;
}

void LiteratureSearch::setProgressCallback(ProgressCallback callback) {
    pImpl->progressCallback = callback;
}

std::vector<BibEntry> LiteratureSearch::search(const SearchQuery& query, SearchEngine engine) {
    std::vector<BibEntry> allResults;
    pImpl->cancelFlag = false;

    auto searchSingleEngine = [&](SearchEngine eng) {
        if (pImpl->cancelFlag) return;

        std::vector<BibEntry> results;
        switch (eng) {
            case SearchEngine::PubMed:
                results = pImpl->searchPubMed(query);
                break;
            case SearchEngine::ArXiv:
                results = pImpl->searchArXiv(query);
                break;
            case SearchEngine::CrossRef:
                results = pImpl->searchCrossRef(query);
                break;
            case SearchEngine::SemanticScholar:
                results = pImpl->searchSemanticScholar(query);
                break;
            default:
                break;
        }

        for (const auto& entry : results) {
            if (pImpl->resultCallback) {
                pImpl->resultCallback(entry);
            }
            allResults.push_back(entry);
        }
    };

    if (engine == SearchEngine::All) {
        // Search all engines
        searchSingleEngine(SearchEngine::PubMed);
        searchSingleEngine(SearchEngine::ArXiv);
        searchSingleEngine(SearchEngine::CrossRef);
        searchSingleEngine(SearchEngine::SemanticScholar);
    } else {
        searchSingleEngine(engine);
    }

    return allResults;
}

void LiteratureSearch::searchAsync(const SearchQuery& query, SearchEngine engine) {
    std::thread searchThread([this, query, engine]() {
        search(query, engine);
    });
    searchThread.detach();
}

void LiteratureSearch::cancelSearch() {
    pImpl->cancelFlag = true;
}

std::vector<std::string> LiteratureSearch::getAvailableEngines() {
    return {"PubMed", "ArXiv", "CrossRef", "SemanticScholar"};
    // Note: GoogleScholar, IEEE, and ACM are not yet implemented
    // GoogleScholar is particularly difficult due to lack of official API and anti-bot measures
    // IEEE and ACM require API keys
}

std::string LiteratureSearch::engineToString(SearchEngine engine) {
    switch (engine) {
        case SearchEngine::PubMed: return "PubMed";
        case SearchEngine::ArXiv: return "ArXiv";
        case SearchEngine::GoogleScholar: return "GoogleScholar";
        case SearchEngine::CrossRef: return "CrossRef";
        case SearchEngine::IEEE: return "IEEE";
        case SearchEngine::ACM: return "ACM";
        case SearchEngine::SemanticScholar: return "SemanticScholar";
        case SearchEngine::All: return "All";
        default: return "Unknown";
    }
}

SearchEngine LiteratureSearch::stringToEngine(const std::string& engine) {
    std::string lower = engine;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "pubmed") return SearchEngine::PubMed;
    if (lower == "arxiv") return SearchEngine::ArXiv;
    if (lower == "googlescholar" || lower == "scholar") return SearchEngine::GoogleScholar;
    if (lower == "crossref" || lower == "doi") return SearchEngine::CrossRef;
    if (lower == "ieee") return SearchEngine::IEEE;
    if (lower == "acm") return SearchEngine::ACM;
    if (lower == "semanticscholar" || lower == "semantic") return SearchEngine::SemanticScholar;
    if (lower == "all") return SearchEngine::All;

    return SearchEngine::All;
}

// Utility functions
std::string Utils::entriesToJSON(const std::vector<BibEntry>& entries) {
    Json::Value root;
    for (const auto& entry : entries) {
        Json::Value entryJson;
        entryJson["id"] = entry.id;
        entryJson["type"] = entry.type;
        for (const auto& [key, value] : entry.fields) {
            entryJson["fields"][key] = value;
        }
        root.append(entryJson);
    }
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string Utils::entriesToBibTeX(const std::vector<BibEntry>& entries) {
    std::ostringstream oss;
    for (const auto& entry : entries) {
        oss << entry.toBibTeX() << "\n";
    }
    return oss.str();
}

std::vector<BibEntry> Utils::parseBibTeX(const std::string& bibtex) {
    std::vector<BibEntry> entries;
    // Simplified BibTeX parsing - in production use a proper parser
    // This is a placeholder implementation
    (void)bibtex; // Suppress unused parameter warning
    return entries;
}

} // namespace BibSearch