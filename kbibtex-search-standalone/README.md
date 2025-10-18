# KBibTeX Literature Search - Standalone Library

A lightweight, standalone literature search library extracted from KBibTeX. This library provides command-line and Python interfaces for searching academic literature across multiple databases including PubMed, ArXiv, and CrossRef.

## Features

- **Multi-database search**: Support for PubMed, ArXiv, CrossRef, and more
- **Multiple interfaces**: Command-line tool, C++ library, and Python module
- **Flexible search options**: Search by author, title, keywords, and year
- **Multiple output formats**: BibTeX and JSON
- **No GUI dependencies**: Pure command-line interface
- **Lightweight**: Minimal dependencies compared to full KBibTeX

## Quick Start

### Python Usage

The simplest way to use the search functionality:

```python
import bibsearch

# Simple search returning JSON
results = bibsearch.search(
    keywords="deep learning",
    max_results=5,
    engine="semanticscholar"
)

# Print results
import json
print(json.dumps(results, indent=2))

# Get results as BibTeX
bibtex = bibsearch.search_to_bibtex(
    keywords="quantum computing",
    max_results=10,
    engine="arxiv"
)
print(bibtex)
```

### Command-Line Usage

```bash
# Search PubMed for papers by an author
kbibtex-search -a "Smith J" -y 2023 -e pubmed

# Search ArXiv for quantum computing papers
kbibtex-search -k "quantum computing" -e arxiv -n 20

# Search all databases and save to file
kbibtex-search -t "machine learning" -f bibtex -o results.bib

# Search with multiple criteria
kbibtex-search -a "Johnson" -t "COVID" -y 2020 -e pubmed -f json
```

### Python Library API

```python
from bibsearch import LiteratureSearch, SearchQuery, SearchEngine

# Create search instance
searcher = LiteratureSearch()

# Define search query
query = SearchQuery(
    author="Feynman",
    title="quantum",
    max_results=10
)

# Perform search
results = searcher.search(query, SearchEngine.ALL)

# Process results
for entry in results:
    print(f"Title: {entry.title}")
    print(f"Authors: {entry.authors}")
    print(f"Year: {entry.year}")
    print(f"DOI: {entry.doi}")
    print("---")
```

## Installation

### From Source (C++)

```bash
# Clone repository
git clone <repository-url>
cd kbibtex-search-standalone

# Build with CMake
mkdir build
cd build
cmake ..
make
sudo make install
```

### Python Module

```bash
# Install Python module
cd python
pip install -e .

# Or install with dependencies
pip install -r requirements.txt
pip install -e .
```

### Dependencies

**C++ Version:**
- libcurl (HTTP requests)
- jsoncpp (JSON parsing)
- C++17 compiler

**Python Version:**
- Python 3.6+
- No external dependencies (uses urllib and json from standard library)

## API Reference

### Python Classes

#### `SearchQuery`
```python
@dataclass
class SearchQuery:
    author: Optional[str] = None
    title: Optional[str] = None
    keywords: Optional[str] = None
    year: Optional[str] = None
    max_results: int = 10
```

#### `BibEntry`
```python
@dataclass
class BibEntry:
    id: str
    type: str = "article"  # article, book, inproceedings, etc.
    fields: Dict[str, str]  # title, author, year, doi, abstract, etc.

    # Methods
    to_bibtex() -> str
    to_dict() -> Dict[str, Any]
```

#### `LiteratureSearch`
```python
class LiteratureSearch:
    def search(query: SearchQuery, engine: SearchEngine) -> List[BibEntry]
    def searchAsync(query: SearchQuery, engine: SearchEngine) -> None
    def set_callbacks(result_callback, error_callback, progress_callback)
```

### Command-Line Options

```
kbibtex-search [OPTIONS]

Search options:
  -a, --author AUTHOR       Search by author name
  -t, --title TITLE         Search by title
  -k, --keywords KEYWORDS   Search by keywords (free text)
  -y, --year YEAR          Search by publication year
  -n, --max-results NUM    Maximum results per engine (default: 10)

Engine options:
  -e, --engine ENGINE      Search engine: pubmed, arxiv, crossref, all
  --list-engines          List available search engines

Output options:
  -o, --output FILE       Output file
  -f, --format FORMAT     Output format: bibtex, json (default: bibtex)
  -q, --quiet            Suppress progress messages
```

## Examples

### Example 1: Search for COVID-19 papers from 2020

```python
import bibsearch

results = bibsearch.search(
    keywords="COVID-19 vaccine",
    year="2020",
    max_results=20,
    engine="pubmed"
)

for result in results:
    print(f"{result['fields']['title']}")
    print(f"Authors: {result['fields'].get('author', 'N/A')}")
    print(f"DOI: {result['fields'].get('doi', 'N/A')}\n")
```

### Example 2: Build a bibliography file

```python
import bibsearch

# Search multiple topics
topics = [
    {"keywords": "deep learning", "year": "2023"},
    {"author": "LeCun", "title": "neural"},
    {"keywords": "transformer architecture", "engine": "arxiv"}
]

all_bibtex = []
for topic in topics:
    engine = topic.pop("engine", "all")
    bibtex = bibsearch.search_to_bibtex(**topic, engine=engine)
    all_bibtex.append(bibtex)

# Save to file
with open("bibliography.bib", "w") as f:
    f.write("\n\n".join(all_bibtex))
```

### Example 3: Custom result processing

```python
from bibsearch import LiteratureSearch, SearchQuery, SearchEngine

searcher = LiteratureSearch()

# Set up callbacks
searcher.set_callbacks(
    result_callback=lambda entry: print(f"Found: {entry.title}"),
    error_callback=lambda err: print(f"Error: {err}"),
    progress_callback=lambda cur, tot: print(f"Progress: {cur}/{tot}")
)

query = SearchQuery(
    author="Einstein",
    max_results=5
)

results = searcher.search(query, SearchEngine.CROSSREF)
```

## Supported Search Engines

| Engine | Description | Search Fields Supported |
|--------|-------------|------------------------|
| PubMed | Biomedical literature database | Author, Title, Keywords, Year |
| ArXiv | Preprint repository for physics, math, CS | Author, Title, Keywords |
| CrossRef | DOI registration agency | Author, Title, Keywords, Year |
| Google Scholar* | Academic search engine | All fields |
| IEEE* | Engineering and technology | All fields |
| ACM* | Computer science | All fields |
| Semantic Scholar* | AI-powered academic search | All fields |

*Note: Some engines marked with * may require additional implementation or API keys.

## Output Formats

### BibTeX Format
```bibtex
@article{pmid12345678,
    title = {Example Article Title},
    author = {John Smith and Jane Doe},
    year = {2023},
    journal = {Journal Name},
    doi = {10.1234/example},
}
```

### JSON Format
```json
[
  {
    "id": "pmid12345678",
    "type": "article",
    "fields": {
      "title": "Example Article Title",
      "author": "John Smith and Jane Doe",
      "year": "2023",
      "journal": "Journal Name",
      "doi": "10.1234/example"
    }
  }
]
```

## Development

### Building from Source

```bash
# Debug build
mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Release build
mkdir build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Running Tests

```bash
# Python tests
cd python
python -m pytest tests/

# C++ tests (if implemented)
cd build
make test
```

### Contributing

To add a new search engine:

1. Implement the search method in `src/bibsearch.cpp`
2. Add the engine to the `SearchEngine` enum
3. Update the Python wrapper if needed
4. Add tests and documentation

## Limitations

- Some search engines may have rate limits
- Complex queries might not be supported by all engines
- Results are limited to what each API provides
- No authentication support for restricted databases
- **Historical papers**: Papers published before ~1990 may not be well-indexed in modern databases. For example, Einstein's 1905 papers won't be found by year search. Instead, search by **title** (e.g., `title="Electrodynamics of Moving Bodies"`) to find reprints and highly-cited versions

## License

This project is licensed under GPL-2.0-or-later, consistent with the original KBibTeX project.

## Acknowledgments

This standalone library is extracted from the KBibTeX project. Original search functionality was implemented by the KBibTeX team.

## Support

For issues, questions, or contributions, please refer to the main KBibTeX project repository.