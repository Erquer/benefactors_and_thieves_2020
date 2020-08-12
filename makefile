SOURCES := $(shell find . -name '*.cpp')
HEADERS := $(shell find . -name '*.h')
OUTPUT_FILE := thieves_benefactors

all: clean $(OUTPUT_FILE)

$(OUTPUT_FILE): $(SOURCES) $(HEADERS)
	mpiCC -Wall -pthread -o $(OUTPUT_FILE) $(SOURCES)

clean:
	$(RM) $(OUTPUT_FILE)
