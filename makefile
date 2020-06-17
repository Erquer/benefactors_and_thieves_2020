SOURCES := $(shell find . -name '*.cpp')
HEADERS := $(shell find . -name '*.h')
OUTPUT_FILE := benefactors_thieves

all: clean $(OUTPUT_FILE)

$(OUTPUT_FILE): $(SOURCES) $(HEADERS)
    mpicc -o $(OUTPUT_FILE) $(SOURCES)

clean:
    $(RM) $(OUTPUT_FILE)