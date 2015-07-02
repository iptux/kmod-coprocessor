
PANDOC = pandoc

MARKDOWN = $(shell find -name '*.md')
HTML = $(patsubst %.md,%.html,$(MARKDOWN))

all: $(HTML)
.PHONY: all

clean:
	-$(RM) $(HTML)
.PHONY: clean

%.html: %.md
	$(PANDOC) --from markdown --to html --output $@ $<

pack: all
	zip -r -j coprocessor-$(shell date +%F).zip * -x '*.md' '*.zip' Makefile TODO
.PHONY:pack

