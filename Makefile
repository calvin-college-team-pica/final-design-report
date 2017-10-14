LATEX := pdflatex
BIBTEX := bibtex
FILENAME  := "Final_Design_Report"
TEXFILE := $(FILENAME).tex

all: bibtex
	@echo "Running LaTeX once..."
	$(LATEX) $(TEXFILE) > /dev/null
	@echo "Running it again, grepping for Warnings"
	$(LATEX) $(TEXFILE)

clean:
	rm -f *.aux *.toc *.lol *.lof *.lot *.bbl *.blg

editor-clean:
	@- $(RM) $(wildcard *~)

distclean: clean editor-clean

bootstrap:
	$(LATEX) bootstrap.tex

latex:
	$(LATEX) $(TEXFILE)

bibtex: latex
	$(BIBTEX) $(FILENAME)
