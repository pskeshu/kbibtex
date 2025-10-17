# Makefile for KBibTeX Search Standalone

.PHONY: all build clean install test python-install

# Default target
all: build

# Build C++ binary
build:
	@echo "Building C++ binary..."
	@mkdir -p build
	@cd build && cmake .. && make

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build
	@rm -f python/*.pyc python/__pycache__
	@rm -f examples/*.bib examples/*.json

# Install C++ binary
install: build
	@echo "Installing C++ binary..."
	@cd build && sudo make install

# Install Python module
python-install:
	@echo "Installing Python module..."
	@cd python && pip install -e .

# Run tests
test:
	@echo "Running Python tests..."
	@cd python && python bibsearch.py --help
	@echo "Testing example search..."
	@cd python && python bibsearch.py -k "test" -n 1 -e arxiv

# Run examples
examples:
	@echo "Running examples..."
	@cd examples && python example_search.py

# Build documentation
docs:
	@echo "Building documentation..."
	@cd build && make doc

# Quick test of the Python module
python-test:
	@echo "Quick test of Python module..."
	@python3 -c "import sys; sys.path.insert(0, 'python'); import bibsearch; print(bibsearch.search(keywords='test', max_results=1, engine='arxiv'))"

# Help target
help:
	@echo "KBibTeX Search Standalone - Make targets:"
	@echo "  make build         - Build C++ binary"
	@echo "  make install       - Install C++ binary (requires sudo)"
	@echo "  make python-install - Install Python module"
	@echo "  make test          - Run basic tests"
	@echo "  make examples      - Run example scripts"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make docs          - Build documentation (requires Doxygen)"
	@echo "  make help          - Show this help message"