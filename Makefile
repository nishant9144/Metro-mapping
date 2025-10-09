CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2

ENCODER_EXE = encoder
DECODER_EXE = decoder

PYTHON ?= python3

FILE ?= mytest
VIZFLAGS ?=
.PHONY: all run clean viz

all: $(ENCODER_EXE) $(DECODER_EXE)

build:
	./compile.sh

run: all
	./run1.sh $(FILE)
	@minisat $(FILE).satinput $(FILE).satoutput; \
	status=$$?; \
	if [ $$status -ne 10 ] && [ $$status -ne 20 ]; then \
		echo "minisat failed"; \
		exit $$status; \
	fi
	./run2.sh $(FILE)

clean:
	rm -f $(ENCODER_EXE) $(DECODER_EXE)
	rm -f *.satinput *.satoutput *.metromap

$(ENCODER_EXE): encoder.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(DECODER_EXE): decoder.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

viz:
	@if [ ! -f "$(FILE).city" ]; then echo "Missing $(FILE).city"; exit 1; fi
	@if [ ! -f "$(FILE).metromap" ]; then echo "Missing $(FILE).metromap"; exit 1; fi
	@$(PYTHON) visualizer.py $(FILE).city $(FILE).metromap $(VIZFLAGS)


# make run FILE=sample