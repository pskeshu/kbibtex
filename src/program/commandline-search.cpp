/***************************************************************************
 *   SPDX-License-Identifier: GPL-2.0-or-later
 *                                                                         *
 *   SPDX-FileCopyrightText: 2024 KBibTeX CLI Search Tool                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <https://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include <iostream>
#include <memory>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QEventLoop>
#include <QVector>

#include "kbibtex-version.h"
#include <Entry>
#include <File>
#include <FileExporterBibTeX>

// Include all available online search engines
#include <onlinesearch/onlinesearchabstract.h>
#include <onlinesearch/onlinesearchpubmed.h>
#include <onlinesearch/onlinesearcharxiv.h>
#include <onlinesearch/onlinesearchgooglescholar.h>
#include <onlinesearch/onlinesearchdoi.h>
#include <onlinesearch/onlinesearchacmportal.h>
#include <onlinesearch/onlinesearchieee.h>
#include <onlinesearch/onlinesearchspringerlink.h>
#include <onlinesearch/onlinesearchsciencedirect.h>
#include <onlinesearch/onlinesearchcernds.h>
#include <onlinesearch/onlinesearchbibsonomy.h>
#include <onlinesearch/onlinesearchingentaconnect.h>
#include <onlinesearch/onlinesearchsoanasaads.h>
#include <onlinesearch/onlinesearchmathscinet.h>
#include <onlinesearch/onlinesearchmrlookup.h>
#include <onlinesearch/onlinesearchinspirehep.h>
#include <onlinesearch/onlinesearchideasrepec.h>
#include <onlinesearch/onlinesearchzbmath.h>
#include <onlinesearch/onlinesearchbiorxiv.h>
#include <onlinesearch/onlinesearchsemanticscholar.h>
#include <onlinesearch/onlinesearchunpaywall.h>

class SearchManager : public QObject
{
    Q_OBJECT

public:
    SearchManager(QObject *parent = nullptr)
        : QObject(parent), m_totalResults(0), m_activeSearches(0), m_hasErrors(false) {}

    void addSearchEngine(const QString &engineName) {
        OnlineSearchAbstract *engine = nullptr;

        // Create the appropriate search engine based on name
        if (engineName.toLower() == "pubmed") {
            engine = new OnlineSearchPubMed(this);
        } else if (engineName.toLower() == "arxiv") {
            engine = new OnlineSearchArXiv(this);
        } else if (engineName.toLower() == "googlescholar" || engineName.toLower() == "scholar") {
            engine = new OnlineSearchGoogleScholar(this);
        } else if (engineName.toLower() == "doi") {
            engine = new OnlineSearchDOI(this);
        } else if (engineName.toLower() == "acm") {
            engine = new OnlineSearchAcmPortal(this);
        } else if (engineName.toLower() == "ieee") {
            engine = new OnlineSearchIEEEXplore(this);
        } else if (engineName.toLower() == "springer") {
            engine = new OnlineSearchSpringerLink(this);
        } else if (engineName.toLower() == "sciencedirect") {
            engine = new OnlineSearchScienceDirect(this);
        } else if (engineName.toLower() == "cernds" || engineName.toLower() == "cern") {
            engine = new OnlineSearchCERNDS(this);
        } else if (engineName.toLower() == "bibsonomy") {
            engine = new OnlineSearchBibsonomy(this);
        } else if (engineName.toLower() == "ingenta") {
            engine = new OnlineSearchIngentaConnect(this);
        } else if (engineName.toLower() == "ads" || engineName.toLower() == "nasa") {
            engine = new OnlineSearchSOANASAADS(this);
        } else if (engineName.toLower() == "mathscinet") {
            engine = new OnlineSearchMathSciNet(this);
        } else if (engineName.toLower() == "mrlookup") {
            engine = new OnlineSearchMRLookup(this);
        } else if (engineName.toLower() == "inspirehep" || engineName.toLower() == "inspire") {
            engine = new OnlineSearchInspireHep(this);
        } else if (engineName.toLower() == "ideas" || engineName.toLower() == "repec") {
            engine = new OnlineSearchIDEASRePEc(this);
        } else if (engineName.toLower() == "zbmath") {
            engine = new OnlineSearchzbMath(this);
        } else if (engineName.toLower() == "biorxiv") {
            engine = new OnlineSearchBioRxiv(OnlineSearchBioRxiv::Rxiv::bioRxiv, this);
        } else if (engineName.toLower() == "medrxiv") {
            engine = new OnlineSearchBioRxiv(OnlineSearchBioRxiv::Rxiv::medRxiv, this);
        } else if (engineName.toLower() == "semanticscholar" || engineName.toLower() == "semantic") {
            engine = new OnlineSearchSemanticScholar(this);
        } else if (engineName.toLower() == "unpaywall") {
            engine = new OnlineSearchUnpaywall(this);
        } else {
            std::cerr << "Unknown search engine: " << engineName.toStdString() << std::endl;
            return;
        }

        if (engine) {
            m_engines.append(engine);
            connect(engine, &OnlineSearchAbstract::foundEntry,
                    this, &SearchManager::handleFoundEntry);
            connect(engine, &OnlineSearchAbstract::stoppedSearch,
                    this, &SearchManager::handleSearchStopped);
            connect(engine, &OnlineSearchAbstract::progress,
                    this, &SearchManager::handleProgress);
        }
    }

    void startSearch(const QMap<OnlineSearchAbstract::QueryKey, QString> &query, int numResults) {
        if (m_engines.isEmpty()) {
            std::cerr << "No search engines configured" << std::endl;
            return;
        }

        m_activeSearches = m_engines.size();
        m_totalResults = 0;
        m_hasErrors = false;

        std::cerr << "Starting search with " << m_engines.size() << " engine(s)..." << std::endl;

        // Start all searches
        for (OnlineSearchAbstract *engine : m_engines) {
            std::cerr << "  - Searching " << engine->label().toStdString() << "..." << std::endl;
            engine->startSearch(query, numResults);
        }
    }

    bool hasErrors() const { return m_hasErrors; }

    const QVector<QSharedPointer<Entry>>& results() const { return m_results; }

Q_SIGNALS:
    void allSearchesCompleted();

private Q_SLOTS:
    void handleFoundEntry(QSharedPointer<Entry> entry) {
        if (!entry.isNull()) {
            m_results.append(entry);
            m_totalResults++;

            // Output progress to stderr
            OnlineSearchAbstract *engine = qobject_cast<OnlineSearchAbstract*>(sender());
            if (engine) {
                std::cerr << "    Found entry from " << engine->label().toStdString()
                          << ": " << entry->id().toStdString() << std::endl;
            }
        }
    }

    void handleSearchStopped(int errorCode) {
        OnlineSearchAbstract *engine = qobject_cast<OnlineSearchAbstract*>(sender());
        if (engine) {
            if (errorCode != OnlineSearchAbstract::resultNoError &&
                errorCode != OnlineSearchAbstract::resultCancelled) {
                m_hasErrors = true;
                std::cerr << "    Error in " << engine->label().toStdString()
                          << " (code: " << errorCode << ")" << std::endl;
            } else {
                std::cerr << "    Completed " << engine->label().toStdString() << std::endl;
            }
        }

        m_activeSearches--;
        if (m_activeSearches <= 0) {
            std::cerr << "\nSearch completed. Found " << m_totalResults << " result(s)." << std::endl;
            Q_EMIT allSearchesCompleted();
        }
    }

    void handleProgress(int current, int total) {
        // Could implement a progress bar here if desired
        Q_UNUSED(current)
        Q_UNUSED(total)
    }

private:
    QVector<OnlineSearchAbstract*> m_engines;
    QVector<QSharedPointer<Entry>> m_results;
    int m_totalResults;
    int m_activeSearches;
    bool m_hasErrors;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kbibtex-search"));
    app.setApplicationVersion(QStringLiteral(KBIBTEX_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("KBibTeX Command-Line Literature Search Tool"));
    parser.addHelpOption();
    parser.addVersionOption();

    // Search parameter options
    QCommandLineOption authorOption("a", "Search by author name", "author");
    parser.addOption(authorOption);

    QCommandLineOption titleOption("t", "Search by title", "title");
    parser.addOption(titleOption);

    QCommandLineOption keywordsOption("k", "Search by keywords/free text", "keywords");
    parser.addOption(keywordsOption);

    QCommandLineOption yearOption("y", "Search by year", "year");
    parser.addOption(yearOption);

    // Search engine selection
    QCommandLineOption enginesOption("e", "Search engines to use (comma-separated)\n"
        "Available: pubmed, arxiv, googlescholar, doi, acm, ieee, springer,\n"
        "           sciencedirect, cern, bibsonomy, ingenta, ads, mathscinet,\n"
        "           mrlookup, inspire, ideas, zbmath, biorxiv, medrxiv,\n"
        "           semanticscholar, unpaywall", "engines");
    parser.addOption(enginesOption);

    // Results options
    QCommandLineOption numResultsOption("n", "Maximum number of results per engine (default: 10)", "number", "10");
    parser.addOption(numResultsOption);

    QCommandLineOption outputOption("o", "Output file (BibTeX format)", "file");
    parser.addOption(outputOption);

    QCommandLineOption listEnginesOption("list-engines", "List all available search engines");
    parser.addOption(listEnginesOption);

    parser.process(app);

    // Handle list engines option
    if (parser.isSet(listEnginesOption)) {
        std::cout << "Available search engines:" << std::endl;
        std::cout << "  - pubmed         : PubMed/MEDLINE database" << std::endl;
        std::cout << "  - arxiv          : arXiv preprint repository" << std::endl;
        std::cout << "  - googlescholar  : Google Scholar" << std::endl;
        std::cout << "  - doi            : CrossRef DOI resolver" << std::endl;
        std::cout << "  - acm            : ACM Digital Library" << std::endl;
        std::cout << "  - ieee           : IEEE Xplore" << std::endl;
        std::cout << "  - springer       : SpringerLink" << std::endl;
        std::cout << "  - sciencedirect  : ScienceDirect" << std::endl;
        std::cout << "  - cern           : CERN Document Server" << std::endl;
        std::cout << "  - bibsonomy      : BibSonomy" << std::endl;
        std::cout << "  - ingenta        : Ingenta Connect" << std::endl;
        std::cout << "  - ads            : NASA Astrophysics Data System" << std::endl;
        std::cout << "  - mathscinet     : MathSciNet" << std::endl;
        std::cout << "  - mrlookup       : MR Lookup" << std::endl;
        std::cout << "  - inspire        : INSPIRE-HEP" << std::endl;
        std::cout << "  - ideas          : IDEAS RePEc" << std::endl;
        std::cout << "  - zbmath         : zbMATH" << std::endl;
        std::cout << "  - biorxiv        : bioRxiv preprint server" << std::endl;
        std::cout << "  - medrxiv        : medRxiv preprint server" << std::endl;
        std::cout << "  - semanticscholar: Semantic Scholar" << std::endl;
        std::cout << "  - unpaywall      : Unpaywall" << std::endl;
        return 0;
    }

    // Check if at least one search parameter is provided
    if (!parser.isSet(authorOption) && !parser.isSet(titleOption) &&
        !parser.isSet(keywordsOption) && !parser.isSet(yearOption)) {
        std::cerr << "Error: At least one search parameter must be provided (-a, -t, -k, or -y)" << std::endl;
        std::cerr << "Use --help for more information" << std::endl;
        return 1;
    }

    // Build search query
    QMap<OnlineSearchAbstract::QueryKey, QString> query;
    if (parser.isSet(authorOption))
        query[OnlineSearchAbstract::QueryKey::Author] = parser.value(authorOption);
    if (parser.isSet(titleOption))
        query[OnlineSearchAbstract::QueryKey::Title] = parser.value(titleOption);
    if (parser.isSet(keywordsOption))
        query[OnlineSearchAbstract::QueryKey::FreeText] = parser.value(keywordsOption);
    if (parser.isSet(yearOption))
        query[OnlineSearchAbstract::QueryKey::Year] = parser.value(yearOption);

    // Get number of results
    bool ok;
    int numResults = parser.value(numResultsOption).toInt(&ok);
    if (!ok || numResults <= 0) {
        std::cerr << "Error: Invalid number of results specified" << std::endl;
        return 1;
    }

    // Create search manager
    SearchManager searchManager(&app);

    // Configure search engines
    QString enginesList = parser.value(enginesOption);
    if (enginesList.isEmpty()) {
        // Default to some common engines if none specified
        enginesList = "pubmed,arxiv,googlescholar";
        std::cerr << "No engines specified, using defaults: " << enginesList.toStdString() << std::endl;
    }

    QStringList engines = enginesList.split(',', Qt::SkipEmptyParts);
    for (const QString &engine : engines) {
        searchManager.addSearchEngine(engine.trimmed());
    }

    // Set up event loop
    QEventLoop eventLoop;
    QObject::connect(&searchManager, &SearchManager::allSearchesCompleted,
                     &eventLoop, &QEventLoop::quit);

    // Start the search
    searchManager.startSearch(query, numResults);

    // Run event loop until all searches complete
    eventLoop.exec();

    // Process results
    const auto &results = searchManager.results();
    if (results.isEmpty()) {
        std::cerr << "No results found" << std::endl;
        return 0;
    }

    // Create a File object to hold all entries
    File bibtexFile;
    for (const auto &entry : results) {
        if (!entry.isNull()) {
            bibtexFile.append(entry);
        }
    }

    // Export results
    FileExporterBibTeX exporter(&app);
    if (parser.isSet(outputOption)) {
        // Save to file
        QString outputPath = parser.value(outputOption);
        QFile outputFile(outputPath);
        if (outputFile.open(QFile::WriteOnly)) {
            if (exporter.save(&outputFile, &bibtexFile)) {
                std::cerr << "Results saved to " << outputPath.toStdString() << std::endl;
            } else {
                std::cerr << "Error: Failed to save results to file" << std::endl;
                return 1;
            }
            outputFile.close();
        } else {
            std::cerr << "Error: Cannot open output file for writing" << std::endl;
            return 1;
        }
    } else {
        // Output to stdout
        const QString output = exporter.toString(&bibtexFile);
        std::cout << output.toStdString() << std::endl;
    }

    return searchManager.hasErrors() ? 1 : 0;
}

#include "commandline-search.moc"