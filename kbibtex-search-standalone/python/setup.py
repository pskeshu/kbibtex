#!/usr/bin/env python3
"""
Setup script for the KBibTeX Literature Search Python module.
"""

from setuptools import setup, find_packages

with open("../README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="kbibtex-search",
    version="1.0.0",
    author="KBibTeX Contributors",
    description="A lightweight literature search library for academic databases",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/kbibtex/kbibtex-search",
    py_modules=["bibsearch"],
    python_requires=">=3.6",
    install_requires=[
        # Core functionality works with standard library only
        # Optional dependencies for enhanced features:
        # "requests>=2.25.0",  # Better HTTP handling
        # "lxml>=4.6.0",       # Better XML parsing
        # "beautifulsoup4>=4.9.0",  # HTML parsing
    ],
    extras_require={
        "dev": [
            "pytest>=6.0.0",
            "pytest-cov>=2.10.0",
            "black>=20.8b1",
            "flake8>=3.8.0",
        ],
        "enhanced": [
            "requests>=2.25.0",
            "lxml>=4.6.0",
            "beautifulsoup4>=4.9.0",
        ]
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Science/Research",
        "Topic :: Scientific/Engineering :: Information Analysis",
        "License :: OSI Approved :: GNU General Public License v2 or later (GPLv2+)",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Operating System :: OS Independent",
    ],
    entry_points={
        "console_scripts": [
            "bibsearch=bibsearch:main",
            "kbibtex-search-py=bibsearch:main",
        ],
    },
    keywords="bibtex, bibliography, literature, search, pubmed, arxiv, crossref",
    project_urls={
        "Bug Reports": "https://github.com/kbibtex/kbibtex-search/issues",
        "Source": "https://github.com/kbibtex/kbibtex-search",
        "Documentation": "https://github.com/kbibtex/kbibtex-search/wiki",
    },
)