LATEX ?= xelatex
MAIN := main
OUTDIR := build
LATEX_FLAGS := -interaction=nonstopmode -halt-on-error

.PHONY: all pdf clean

all: pdf

$(OUTDIR):
	mkdir -p $(OUTDIR)

pdf: $(OUTDIR)
	$(LATEX) $(LATEX_FLAGS) -output-directory $(OUTDIR) $(MAIN).tex
	$(LATEX) $(LATEX_FLAGS) -output-directory $(OUTDIR) $(MAIN).tex

clean:
	rm -rf $(OUTDIR)
