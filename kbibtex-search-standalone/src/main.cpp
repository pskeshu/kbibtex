/*
 * KBibTeX Standalone Search CLI
 * Command-line interface for literature search
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include "bibsearch.h"

void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n";
    std::cout << "Literature search command-line tool\n\n";
    std::cout << "Search options:\n";
    std::cout << "  -a, --author AUTHOR       Search by author name\n";
    std::cout << "  -t, --title TITLE         Search by title\n";
    std::cout << "  -k, --keywords KEYWORDS   Search by keywords (free text)\n";
    std::cout << "  -y, --year YEAR          Search by publication year\n";
    std::cout << "  -n, --max-results NUM    Maximum results per engine (default: 10)\n\n";
    std::cout << "Engine options:\n";
    std::cout << "  -e, --engine ENGINE      Search engine to use:\n";
    std::cout << "                          pubmed, arxiv, crossref, all (default: all)\n";
    std::cout << "  --list-engines          List available search engines\n\n";
    std::cout << "Output options:\n";
    std::cout << "  -o, --output FILE       Output file (if not specified, prints to stdout)\n";
    std::cout << "  -f, --format FORMAT     Output format: bibtex, json (default: bibtex)\n";
    std::cout << "  -q, --quiet            Suppress progress messages\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " -a \"Smith J\" -y 2023 -e pubmed\n";
    std::cout << "  " << programName << " -k \"machine learning\" -n 20 -f json -o results.json\n";
    std::cout << "  " << programName << " -t \"quantum computing\" -e arxiv\n";
}

int main(int argc, char* argv[]) {
    BibSearch::SearchQuery query;
    BibSearch::SearchEngine engine = BibSearch::SearchEngine::All;
    std::string outputFile;
    std::string outputFormat = "bibtex";
    bool quiet = false;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--list-engines") {
            std::cout << "Available search engines:\n";
            for (const auto& eng : BibSearch::LiteratureSearch::getAvailableEngines()) {
                std::cout << "  - " << eng << "\n";
            }
            return 0;
        } else if ((arg == "-a" || arg == "--author") && i + 1 < argc) {
            query.author = argv[++i];
        } else if ((arg == "-t" || arg == "--title") && i + 1 < argc) {
            query.title = argv[++i];
        } else if ((arg == "-k" || arg == "--keywords") && i + 1 < argc) {
            query.keywords = argv[++i];
        } else if ((arg == "-y" || arg == "--year") && i + 1 < argc) {
            query.year = argv[++i];
        } else if ((arg == "-n" || arg == "--max-results") && i + 1 < argc) {
            query.maxResults = std::stoi(argv[++i]);
        } else if ((arg == "-e" || arg == "--engine") && i + 1 < argc) {
            engine = BibSearch::LiteratureSearch::stringToEngine(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            outputFile = argv[++i];
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            outputFormat = argv[++i];
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information\n";
            return 1;
        }
    }

    // Check if at least one search parameter is provided
    if (query.author.empty() && query.title.empty() &&
        query.keywords.empty() && query.year.empty()) {
        std::cerr << "Error: At least one search parameter must be provided\n";
        std::cerr << "Use --help for usage information\n";
        return 1;
    }

    // Create search instance
    BibSearch::LiteratureSearch search;

    // Set up callbacks
    if (!quiet) {
        search.setProgressCallback([](int current, int total) {
            std::cerr << "\rProgress: " << current << "/" << total << std::flush;
        });

        search.setErrorCallback([](const std::string& error) {
            std::cerr << "\nError: " << error << "\n";
        });
    }

    // Perform search
    if (!quiet) {
        std::cerr << "Searching";
        if (!query.author.empty()) std::cerr << " author=\"" << query.author << "\"";
        if (!query.title.empty()) std::cerr << " title=\"" << query.title << "\"";
        if (!query.keywords.empty()) std::cerr << " keywords=\"" << query.keywords << "\"";
        if (!query.year.empty()) std::cerr << " year=\"" << query.year << "\"";
        std::cerr << " using " << BibSearch::LiteratureSearch::engineToString(engine) << "...\n";
    }

    std::vector<BibSearch::BibEntry> results = search.search(query, engine);

    if (!quiet) {
        std::cerr << "\nFound " << results.size() << " results\n";
    }

    // Generate output
    std::string output;
    if (outputFormat == "json") {
        output = BibSearch::Utils::entriesToJSON(results);
    } else {
        output = BibSearch::Utils::entriesToBibTeX(results);
    }

    // Write output
    if (!outputFile.empty()) {
        std::ofstream out(outputFile);
        if (!out) {
            std::cerr << "Error: Cannot open output file: " << outputFile << "\n";
            return 1;
        }
        out << output;
        out.close();
        if (!quiet) {
            std::cerr << "Results written to " << outputFile << "\n";
        }
    } else {
        std::cout << output;
    }

    return 0;
}