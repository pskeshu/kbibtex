#!/usr/bin/env python3
"""
KBibTeX Literature Search Python Library

A Python wrapper for literature search functionality supporting multiple
academic databases including PubMed, ArXiv, and CrossRef.
"""

import json
import subprocess
import tempfile
import os
from typing import List, Dict, Optional, Any, Callable
from dataclasses import dataclass, field, asdict
from enum import Enum
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class SearchEngine(Enum):
    """Available search engines."""
    PUBMED = "pubmed"
    ARXIV = "arxiv"
    CROSSREF = "crossref"
    GOOGLE_SCHOLAR = "googlescholar"
    IEEE = "ieee"
    ACM = "acm"
    SEMANTIC_SCHOLAR = "semanticscholar"
    ALL = "all"


@dataclass
class BibEntry:
    """Represents a bibliographic entry."""
    id: str
    type: str = "article"
    fields: Dict[str, str] = field(default_factory=dict)

    @property
    def title(self) -> str:
        """Get the title of the entry."""
        return self.fields.get("title", "")

    @property
    def authors(self) -> str:
        """Get the authors of the entry."""
        return self.fields.get("author", "")

    @property
    def year(self) -> str:
        """Get the publication year."""
        return self.fields.get("year", "")

    @property
    def doi(self) -> str:
        """Get the DOI if available."""
        return self.fields.get("doi", "")

    @property
    def abstract(self) -> str:
        """Get the abstract if available."""
        return self.fields.get("abstract", "")

    @property
    def url(self) -> str:
        """Get the URL if available."""
        return self.fields.get("url", "")

    @property
    def journal(self) -> str:
        """Get the journal name if available."""
        return self.fields.get("journal", "")

    def to_bibtex(self) -> str:
        """Convert entry to BibTeX format."""
        lines = [f"@{self.type}{{{self.id},"]
        for key, value in self.fields.items():
            # Escape special characters in BibTeX
            value = value.replace("{", "\\{").replace("}", "\\}")
            lines.append(f"    {key} = {{{value}}},")
        lines.append("}")
        return "\n".join(lines)

    def to_dict(self) -> Dict[str, Any]:
        """Convert entry to dictionary."""
        return {
            "id": self.id,
            "type": self.type,
            "fields": self.fields
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'BibEntry':
        """Create BibEntry from dictionary."""
        return cls(
            id=data.get("id", ""),
            type=data.get("type", "article"),
            fields=data.get("fields", {})
        )


@dataclass
class SearchQuery:
    """Search query parameters."""
    author: Optional[str] = None
    title: Optional[str] = None
    keywords: Optional[str] = None
    year: Optional[str] = None
    year_from: Optional[str] = None  # Start year for range
    year_to: Optional[str] = None    # End year for range
    max_results: int = 10

    def is_valid(self) -> bool:
        """Check if the query has at least one search parameter."""
        return any([self.author, self.title, self.keywords, self.year, self.year_from, self.year_to])

    def has_year_range(self) -> bool:
        """Check if year range is specified."""
        return self.year_from is not None and self.year_to is not None


class LiteratureSearch:
    """
    Main class for performing literature searches.

    Can use either the compiled C++ binary or implement searches directly in Python.
    """

    def __init__(self, use_binary: bool = False, binary_path: Optional[str] = None):
        """
        Initialize the literature search.

        Args:
            use_binary: If True, use the compiled C++ binary for searches
            binary_path: Path to the search binary (if use_binary is True)
        """
        self.use_binary = use_binary
        self.binary_path = binary_path or "kbibtex-search"

        # Callbacks
        self.result_callback: Optional[Callable[[BibEntry], None]] = None
        self.error_callback: Optional[Callable[[str], None]] = None
        self.progress_callback: Optional[Callable[[int, int], None]] = None

    def set_callbacks(self,
                      result_callback: Optional[Callable[[BibEntry], None]] = None,
                      error_callback: Optional[Callable[[str], None]] = None,
                      progress_callback: Optional[Callable[[int, int], None]] = None):
        """Set callback functions for search events."""
        if result_callback:
            self.result_callback = result_callback
        if error_callback:
            self.error_callback = error_callback
        if progress_callback:
            self.progress_callback = progress_callback

    def search(self, query: SearchQuery,
               engine: SearchEngine = SearchEngine.ALL) -> List[BibEntry]:
        """
        Perform a literature search.

        Args:
            query: Search query parameters
            engine: Search engine to use

        Returns:
            List of bibliographic entries
        """
        if not query.is_valid():
            raise ValueError("Query must have at least one search parameter")

        if self.use_binary:
            return self._search_with_binary(query, engine)
        else:
            return self._search_native(query, engine)

    def _search_with_binary(self, query: SearchQuery,
                           engine: SearchEngine) -> List[BibEntry]:
        """Search using the compiled C++ binary."""
        # Build command-line arguments
        cmd = [self.binary_path, "-f", "json", "-q"]

        if query.author:
            cmd.extend(["-a", query.author])
        if query.title:
            cmd.extend(["-t", query.title])
        if query.keywords:
            cmd.extend(["-k", query.keywords])
        if query.year:
            cmd.extend(["-y", query.year])
        if query.max_results:
            cmd.extend(["-n", str(query.max_results)])
        if engine != SearchEngine.ALL:
            cmd.extend(["-e", engine.value])

        try:
            # Run the command
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)

            # Parse JSON output
            if result.stdout:
                data = json.loads(result.stdout)
                entries = [BibEntry.from_dict(item) for item in data]
                return entries
            else:
                return []

        except subprocess.CalledProcessError as e:
            error_msg = f"Search failed: {e.stderr}"
            logger.error(error_msg)
            if self.error_callback:
                self.error_callback(error_msg)
            return []
        except json.JSONDecodeError as e:
            error_msg = f"Failed to parse search results: {e}"
            logger.error(error_msg)
            if self.error_callback:
                self.error_callback(error_msg)
            return []

    def _matches_year_filter(self, entry: BibEntry, query: SearchQuery) -> bool:
        """Check if an entry matches the year filter."""
        # If no year filter at all, accept everything
        if not query.has_year_range() and not query.year:
            return True

        year_str = entry.fields.get("year", "")
        if not year_str:
            return False  # Reject if year filter is set but paper has no year

        try:
            entry_year = int(year_str)

            # Check year range if specified
            if query.has_year_range():
                from_year = int(query.year_from)
                to_year = int(query.year_to)
                return from_year <= entry_year <= to_year

            # Check single year if specified
            if query.year:
                query_year = int(query.year)
                return entry_year == query_year

            return True
        except (ValueError, TypeError):
            return False  # Reject if year parsing fails when filter is active

    def _search_native(self, query: SearchQuery,
                      engine: SearchEngine) -> List[BibEntry]:
        """
        Native Python implementation of search.
        This is a simplified implementation - expand as needed.
        """
        import urllib.request
        import urllib.parse
        import xml.etree.ElementTree as ET

        results = []

        if engine in [SearchEngine.PUBMED, SearchEngine.ALL]:
            results.extend(self._search_pubmed_native(query))

        if engine in [SearchEngine.ARXIV, SearchEngine.ALL]:
            results.extend(self._search_arxiv_native(query))

        if engine in [SearchEngine.CROSSREF, SearchEngine.ALL]:
            results.extend(self._search_crossref_native(query))

        if engine in [SearchEngine.SEMANTIC_SCHOLAR, SearchEngine.ALL]:
            results.extend(self._search_semanticscholar_native(query))

        return results

    def _search_pubmed_native(self, query: SearchQuery) -> List[BibEntry]:
        """Native Python implementation of PubMed search."""
        import urllib.request
        import urllib.parse
        import xml.etree.ElementTree as ET

        # Build search term
        terms = []
        if query.author:
            terms.append(f"{query.author}[Author]")
        if query.title:
            terms.append(f"{query.title}[Title]")
        if query.keywords:
            terms.append(f"{query.keywords}[All Fields]")
        # Handle year or year range
        if query.has_year_range():
            terms.append(f"{query.year_from}:{query.year_to}[pdat]")
        elif query.year:
            terms.append(f"{query.year}[pdat]")

        if not terms:
            return []

        search_term = " AND ".join(terms)
        search_term = urllib.parse.quote(search_term)

        # Search for IDs
        search_url = (f"https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi?"
                     f"db=pubmed&retmode=json&retmax={query.max_results}&term={search_term}")

        try:
            with urllib.request.urlopen(search_url) as response:
                data = json.loads(response.read().decode())

            id_list = data.get("esearchresult", {}).get("idlist", [])
            if not id_list:
                return []

            # Fetch details
            ids = ",".join(id_list)
            fetch_url = f"https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=pubmed&retmode=xml&id={ids}"

            with urllib.request.urlopen(fetch_url) as response:
                xml_data = response.read().decode()

            # Parse XML
            root = ET.fromstring(xml_data)
            entries = []

            for article in root.findall(".//PubmedArticle"):
                entry = self._parse_pubmed_article(article)
                if entry:
                    entries.append(entry)
                    if self.result_callback:
                        self.result_callback(entry)

            return entries

        except Exception as e:
            error_msg = f"PubMed search failed: {str(e)}"
            logger.error(error_msg)
            if self.error_callback:
                self.error_callback(error_msg)
            return []

    def _parse_pubmed_article(self, article_elem) -> Optional[BibEntry]:
        """Parse a PubMed article XML element."""
        try:
            # Extract PMID
            pmid_elem = article_elem.find(".//PMID")
            if pmid_elem is None:
                return None

            pmid = pmid_elem.text
            entry = BibEntry(id=f"pmid{pmid}", type="article")

            # Extract title
            title_elem = article_elem.find(".//ArticleTitle")
            if title_elem is not None and title_elem.text:
                entry.fields["title"] = title_elem.text

            # Extract authors
            authors = []
            for author in article_elem.findall(".//Author"):
                last_name = author.findtext("LastName", "")
                first_name = author.findtext("ForeName", "")
                if last_name:
                    authors.append(f"{first_name} {last_name}".strip())
            if authors:
                entry.fields["author"] = " and ".join(authors)

            # Extract year
            year_elem = article_elem.find(".//PubDate/Year")
            if year_elem is not None and year_elem.text:
                entry.fields["year"] = year_elem.text

            # Extract journal
            journal_elem = article_elem.find(".//Journal/Title")
            if journal_elem is not None and journal_elem.text:
                entry.fields["journal"] = journal_elem.text

            # Extract abstract
            abstract_elem = article_elem.find(".//AbstractText")
            if abstract_elem is not None and abstract_elem.text:
                entry.fields["abstract"] = abstract_elem.text

            # Extract DOI
            doi_elem = article_elem.find(".//ArticleId[@IdType='doi']")
            if doi_elem is not None and doi_elem.text:
                entry.fields["doi"] = doi_elem.text

            return entry

        except Exception as e:
            logger.warning(f"Failed to parse PubMed article: {e}")
            return None

    def _search_arxiv_native(self, query: SearchQuery) -> List[BibEntry]:
        """Native Python implementation of ArXiv search."""
        import urllib.request
        import urllib.parse
        import xml.etree.ElementTree as ET

        # Build search query
        terms = []
        if query.author:
            terms.append(f"au:{query.author}")
        if query.title:
            terms.append(f"ti:{query.title}")
        if query.keywords:
            terms.append(f"all:{query.keywords}")

        if not terms:
            return []

        search_query = " AND ".join(terms)
        search_query = urllib.parse.quote(search_query)

        url = f"http://export.arxiv.org/api/query?search_query={search_query}&max_results={query.max_results}"

        try:
            with urllib.request.urlopen(url) as response:
                xml_data = response.read().decode()

            # Parse XML (Atom feed)
            root = ET.fromstring(xml_data)
            entries = []

            # Define namespaces
            ns = {"atom": "http://www.w3.org/2005/Atom"}

            for entry_elem in root.findall("atom:entry", ns):
                entry = self._parse_arxiv_entry(entry_elem, ns)
                if entry and self._matches_year_filter(entry, query):
                    entries.append(entry)
                    if self.result_callback:
                        self.result_callback(entry)

            return entries

        except Exception as e:
            error_msg = f"ArXiv search failed: {str(e)}"
            logger.error(error_msg)
            if self.error_callback:
                self.error_callback(error_msg)
            return []

    def _parse_arxiv_entry(self, entry_elem, namespaces) -> Optional[BibEntry]:
        """Parse an ArXiv entry XML element."""
        try:
            # Extract ID
            id_elem = entry_elem.find("atom:id", namespaces)
            if id_elem is None or not id_elem.text:
                return None

            # Extract arXiv ID from URL
            arxiv_id = id_elem.text.split("/abs/")[-1]
            entry = BibEntry(id=f"arxiv{arxiv_id}", type="article")
            entry.fields["eprint"] = arxiv_id
            entry.fields["archivePrefix"] = "arXiv"

            # Extract title
            title_elem = entry_elem.find("atom:title", namespaces)
            if title_elem is not None and title_elem.text:
                # Clean up whitespace in title
                title = " ".join(title_elem.text.split())
                entry.fields["title"] = title

            # Extract authors
            authors = []
            for author_elem in entry_elem.findall("atom:author", namespaces):
                name_elem = author_elem.find("atom:name", namespaces)
                if name_elem is not None and name_elem.text:
                    authors.append(name_elem.text)
            if authors:
                entry.fields["author"] = " and ".join(authors)

            # Extract summary/abstract
            summary_elem = entry_elem.find("atom:summary", namespaces)
            if summary_elem is not None and summary_elem.text:
                # Clean up whitespace
                abstract = " ".join(summary_elem.text.split())
                entry.fields["abstract"] = abstract

            # Extract published date
            published_elem = entry_elem.find("atom:published", namespaces)
            if published_elem is not None and published_elem.text:
                year = published_elem.text[:4]
                entry.fields["year"] = year

            return entry

        except Exception as e:
            logger.warning(f"Failed to parse ArXiv entry: {e}")
            return None

    def _search_crossref_native(self, query: SearchQuery) -> List[BibEntry]:
        """Native Python implementation of CrossRef search."""
        import urllib.request
        import urllib.parse

        # Build query string
        query_parts = []
        if query.title:
            query_parts.append(query.title)
        if query.author:
            query_parts.append(query.author)
        if query.keywords:
            query_parts.append(query.keywords)

        if not query_parts:
            return []

        query_str = " ".join(query_parts)
        query_str = urllib.parse.quote(query_str)

        url = f"https://api.crossref.org/works?query={query_str}&rows={query.max_results}"

        try:
            req = urllib.request.Request(url)
            req.add_header("User-Agent", "KBibTeX-Search/1.0")

            with urllib.request.urlopen(req) as response:
                data = json.loads(response.read().decode())

            items = data.get("message", {}).get("items", [])
            entries = []

            for item in items:
                entry = self._parse_crossref_item(item)
                if entry and self._matches_year_filter(entry, query):
                    entries.append(entry)
                    if self.result_callback:
                        self.result_callback(entry)

            return entries

        except Exception as e:
            error_msg = f"CrossRef search failed: {str(e)}"
            logger.error(error_msg)
            if self.error_callback:
                self.error_callback(error_msg)
            return []

    def _parse_crossref_item(self, item: Dict) -> Optional[BibEntry]:
        """Parse a CrossRef item."""
        try:
            # Extract DOI
            doi = item.get("DOI", "")
            if not doi:
                return None

            entry_id = "doi" + doi.replace("/", "_")
            entry = BibEntry(id=entry_id, type="article")
            entry.fields["doi"] = doi

            # Extract title
            titles = item.get("title", [])
            if titles:
                entry.fields["title"] = titles[0]

            # Extract authors
            authors = []
            for author in item.get("author", []):
                given = author.get("given", "")
                family = author.get("family", "")
                if family:
                    authors.append(f"{given} {family}".strip())
            if authors:
                entry.fields["author"] = " and ".join(authors)

            # Extract year
            date_parts = item.get("published-print", {}).get("date-parts", [[]])
            if date_parts and date_parts[0]:
                entry.fields["year"] = str(date_parts[0][0])

            # Extract journal
            container_title = item.get("container-title", [])
            if container_title:
                entry.fields["journal"] = container_title[0]

            # Extract volume and pages
            if "volume" in item:
                entry.fields["volume"] = item["volume"]
            if "page" in item:
                entry.fields["pages"] = item["page"]

            # Determine entry type
            item_type = item.get("type", "")
            if item_type == "journal-article":
                entry.type = "article"
            elif item_type == "book":
                entry.type = "book"
            elif item_type == "proceedings-article":
                entry.type = "inproceedings"
            else:
                entry.type = "misc"

            return entry

        except Exception as e:
            logger.warning(f"Failed to parse CrossRef item: {e}")
            return None

    def _search_semanticscholar_native(self, query: SearchQuery) -> List[BibEntry]:
        """Native Python implementation of Semantic Scholar search."""
        import urllib.request
        import urllib.parse

        # Build query string
        query_parts = []
        if query.title:
            query_parts.append(query.title)
        if query.author:
            query_parts.append(query.author)
        if query.keywords:
            query_parts.append(query.keywords)

        if not query_parts:
            return []

        query_str = " ".join(query_parts)
        query_str = urllib.parse.quote(query_str)

        url = (f"https://api.semanticscholar.org/graph/v1/paper/search?query={query_str}"
               f"&limit={query.max_results}"
               f"&fields=paperId,title,authors,year,abstract,url,citationCount,venue,publicationTypes")

        try:
            req = urllib.request.Request(url)
            req.add_header("User-Agent", "KBibTeX-Search/1.0")

            with urllib.request.urlopen(req) as response:
                data = json.loads(response.read().decode())

            papers = data.get("data", [])
            entries = []

            for paper in papers:
                entry = self._parse_semanticscholar_paper(paper)
                if entry and self._matches_year_filter(entry, query):
                    entries.append(entry)
                    if self.result_callback:
                        self.result_callback(entry)

            return entries

        except Exception as e:
            error_msg = f"Semantic Scholar search failed: {str(e)}"
            logger.error(error_msg)
            if self.error_callback:
                self.error_callback(error_msg)
            return []

    def _parse_semanticscholar_paper(self, paper: Dict) -> Optional[BibEntry]:
        """Parse a Semantic Scholar paper."""
        try:
            paper_id = paper.get("paperId", "")
            if not paper_id:
                return None

            entry = BibEntry(id=f"s2_{paper_id}", type="article")

            # Extract title
            if "title" in paper:
                entry.fields["title"] = paper["title"]

            # Extract authors
            if "authors" in paper and isinstance(paper["authors"], list):
                authors = []
                for author in paper["authors"]:
                    if "name" in author:
                        authors.append(author["name"])
                if authors:
                    entry.fields["author"] = " and ".join(authors)

            # Extract year
            if "year" in paper and paper["year"]:
                entry.fields["year"] = str(paper["year"])

            # Extract abstract
            if "abstract" in paper and paper["abstract"]:
                entry.fields["abstract"] = paper["abstract"]

            # Extract URL
            if "url" in paper and paper["url"]:
                entry.fields["url"] = paper["url"]

            # Extract venue (journal/conference)
            if "venue" in paper and paper["venue"]:
                entry.fields["journal"] = paper["venue"]

            # Extract citation count
            if "citationCount" in paper and paper["citationCount"] is not None:
                entry.fields["note"] = f"Cited by {paper['citationCount']}"

            # Determine entry type based on publication types
            if "publicationTypes" in paper and isinstance(paper["publicationTypes"], list):
                for pub_type in paper["publicationTypes"]:
                    if pub_type == "Conference":
                        entry.type = "inproceedings"
                        break
                    elif pub_type == "Book":
                        entry.type = "book"
                        break

            return entry

        except Exception as e:
            logger.warning(f"Failed to parse Semantic Scholar paper: {e}")
            return None


# Convenience functions
def search(author: Optional[str] = None,
          title: Optional[str] = None,
          keywords: Optional[str] = None,
          year: Optional[str] = None,
          max_results: int = 10,
          engine: str = "all") -> List[Dict[str, Any]]:
    """
    Convenience function for quick searches.

    Returns results as list of dictionaries for easy JSON serialization.
    """
    query = SearchQuery(
        author=author,
        title=title,
        keywords=keywords,
        year=year,
        max_results=max_results
    )

    search_engine = SearchEngine(engine.lower())
    searcher = LiteratureSearch()
    results = searcher.search(query, search_engine)

    return [entry.to_dict() for entry in results]


def search_to_bibtex(author: Optional[str] = None,
                    title: Optional[str] = None,
                    keywords: Optional[str] = None,
                    year: Optional[str] = None,
                    max_results: int = 10,
                    engine: str = "all") -> str:
    """
    Convenience function that returns results in BibTeX format.
    """
    query = SearchQuery(
        author=author,
        title=title,
        keywords=keywords,
        year=year,
        max_results=max_results
    )

    search_engine = SearchEngine(engine.lower())
    searcher = LiteratureSearch()
    results = searcher.search(query, search_engine)

    return "\n\n".join([entry.to_bibtex() for entry in results])


# CLI interface main function
def main():
    """Main entry point for the command-line interface."""
    import argparse

    parser = argparse.ArgumentParser(
        description="KBibTeX Literature Search Tool - Search academic databases",
        epilog="Example: bibsearch -k 'machine learning' -y 2023 -e arxiv"
    )
    parser.add_argument("-a", "--author", help="Search by author")
    parser.add_argument("-t", "--title", help="Search by title")
    parser.add_argument("-k", "--keywords", help="Search by keywords")
    parser.add_argument("-y", "--year", help="Search by year")
    parser.add_argument("-n", "--max-results", type=int, default=10, help="Maximum results")
    parser.add_argument("-e", "--engine", default="all", help="Search engine to use")
    parser.add_argument("-f", "--format", choices=["json", "bibtex"], default="json",
                       help="Output format")
    parser.add_argument("-o", "--output", help="Output file")
    parser.add_argument("--list-engines", action="store_true", help="List available search engines")

    args = parser.parse_args()

    # List engines if requested
    if args.list_engines:
        print("Available search engines:")
        for engine in SearchEngine:
            print(f"  - {engine.value}")
        return 0

    # Check if at least one search parameter is provided
    if not any([args.author, args.title, args.keywords, args.year]):
        parser.error("At least one search parameter is required (-a, -t, -k, or -y)")

    # Perform search
    try:
        if args.format == "json":
            results = search(
                author=args.author,
                title=args.title,
                keywords=args.keywords,
                year=args.year,
                max_results=args.max_results,
                engine=args.engine
            )
            output = json.dumps(results, indent=2)
        else:
            output = search_to_bibtex(
                author=args.author,
                title=args.title,
                keywords=args.keywords,
                year=args.year,
                max_results=args.max_results,
                engine=args.engine
            )

        # Write output
        if args.output:
            with open(args.output, "w") as f:
                f.write(output)
            print(f"Results written to {args.output}")
        else:
            print(output)

        return 0
    except Exception as e:
        print(f"Error: {e}", file=__import__('sys').stderr)
        return 1


# CLI interface when run as a script
if __name__ == "__main__":
    import sys
    sys.exit(main())