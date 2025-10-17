#!/usr/bin/env python3
"""
Example script demonstrating the KBibTeX search library usage.
"""

import sys
import json
sys.path.insert(0, '../python')

import bibsearch
from bibsearch import LiteratureSearch, SearchQuery, SearchEngine

def example_simple_search():
    """Example 1: Simple search with convenience function."""
    print("=" * 60)
    print("Example 1: Simple Search")
    print("=" * 60)

    # Search for machine learning papers from 2023
    results = bibsearch.search(
        keywords="machine learning",
        year="2023",
        max_results=3,
        engine="arxiv"
    )

    print(f"Found {len(results)} results:\n")
    for i, result in enumerate(results, 1):
        print(f"{i}. {result['fields'].get('title', 'No title')}")
        print(f"   Authors: {result['fields'].get('author', 'N/A')}")
        print(f"   Year: {result['fields'].get('year', 'N/A')}")
        print(f"   ID: {result['id']}\n")


def example_author_search():
    """Example 2: Search by author across multiple databases."""
    print("=" * 60)
    print("Example 2: Author Search")
    print("=" * 60)

    # Search for papers by a specific author
    author_name = "LeCun"  # Example: Yann LeCun

    searcher = LiteratureSearch()
    query = SearchQuery(
        author=author_name,
        max_results=5
    )

    # Search different databases
    for engine in [SearchEngine.ARXIV, SearchEngine.CROSSREF]:
        print(f"\nSearching {engine.value}...")
        results = searcher.search(query, engine)

        if results:
            print(f"Found {len(results)} papers:")
            for entry in results[:3]:  # Show first 3
                print(f"  - {entry.title[:80]}...")
        else:
            print("  No results found")


def example_covid_research():
    """Example 3: Search for COVID-19 research."""
    print("=" * 60)
    print("Example 3: COVID-19 Research Search")
    print("=" * 60)

    # Search PubMed for COVID-19 vaccine papers
    results = bibsearch.search(
        keywords="COVID-19 vaccine efficacy",
        year="2021",
        max_results=5,
        engine="pubmed"
    )

    print(f"\nFound {len(results)} COVID-19 vaccine papers from 2021:\n")
    for result in results:
        fields = result['fields']
        print(f"Title: {fields.get('title', 'N/A')}")
        print(f"Journal: {fields.get('journal', 'N/A')}")
        if 'doi' in fields:
            print(f"DOI: https://doi.org/{fields['doi']}")
        print("-" * 40)


def example_bibtex_export():
    """Example 4: Export search results to BibTeX."""
    print("=" * 60)
    print("Example 4: BibTeX Export")
    print("=" * 60)

    # Search and get results in BibTeX format
    bibtex = bibsearch.search_to_bibtex(
        title="quantum computing",
        max_results=2,
        engine="arxiv"
    )

    print("BibTeX output:")
    print("-" * 40)
    print(bibtex)
    print("-" * 40)

    # Save to file
    filename = "quantum_computing.bib"
    with open(filename, "w") as f:
        f.write(bibtex)
    print(f"\nBibTeX saved to {filename}")


def example_advanced_search():
    """Example 5: Advanced search with callbacks."""
    print("=" * 60)
    print("Example 5: Advanced Search with Progress Tracking")
    print("=" * 60)

    searcher = LiteratureSearch()

    # Set up callbacks for progress tracking
    found_count = 0

    def on_result(entry):
        nonlocal found_count
        found_count += 1
        print(f"  [{found_count}] Found: {entry.title[:60]}...")

    def on_error(error):
        print(f"  ERROR: {error}")

    def on_progress(current, total):
        print(f"  Progress: {current}/{total}")

    searcher.set_callbacks(
        result_callback=on_result,
        error_callback=on_error,
        progress_callback=on_progress
    )

    # Complex search query
    query = SearchQuery(
        author="Einstein",
        keywords="relativity",
        max_results=3
    )

    print(f"Searching for papers by Einstein about relativity...\n")
    results = searcher.search(query, SearchEngine.CROSSREF)

    print(f"\nTotal results found: {len(results)}")


def example_multi_database_comparison():
    """Example 6: Compare results from different databases."""
    print("=" * 60)
    print("Example 6: Multi-Database Comparison")
    print("=" * 60)

    search_term = "artificial intelligence ethics"

    databases = ["arxiv", "crossref", "pubmed"]
    all_results = {}

    print(f"Searching for: '{search_term}'\n")

    for db in databases:
        results = bibsearch.search(
            keywords=search_term,
            max_results=3,
            engine=db
        )
        all_results[db] = results
        print(f"{db.upper()}: Found {len(results)} results")

    # Compare results
    print("\nResults comparison:")
    print("-" * 40)

    for db, results in all_results.items():
        if results:
            print(f"\n{db.upper()} top result:")
            print(f"  {results[0]['fields'].get('title', 'No title')[:70]}...")


def example_json_processing():
    """Example 7: Process results as JSON."""
    print("=" * 60)
    print("Example 7: JSON Processing")
    print("=" * 60)

    # Get results as JSON-ready dictionaries
    results = bibsearch.search(
        keywords="neural networks",
        max_results=2,
        engine="arxiv"
    )

    # Pretty print JSON
    print("JSON output:")
    print(json.dumps(results, indent=2))

    # Save to JSON file
    filename = "search_results.json"
    with open(filename, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nJSON saved to {filename}")


def main():
    """Run all examples."""
    examples = [
        example_simple_search,
        example_author_search,
        example_covid_research,
        example_bibtex_export,
        example_advanced_search,
        example_multi_database_comparison,
        example_json_processing
    ]

    print("\nKBibTeX Literature Search - Examples\n")

    for i, example in enumerate(examples, 1):
        try:
            example()
            print("\n")
        except Exception as e:
            print(f"Example {i} failed: {e}\n")
            continue

        # Optional: pause between examples
        if i < len(examples):
            input("Press Enter to continue to next example...")
            print("\n")


if __name__ == "__main__":
    main()