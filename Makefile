CXX = g++
MONGO_CFLAGS = $(shell pkg-config --cflags libmongoc-1.0)
MONGO_LIBS = $(shell pkg-config --libs libmongoc-1.0)
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -pthread -Iinclude -Ivendor/ntgcalls/include -I/usr/include $(MONGO_CFLAGS)
LDFLAGS = -Lvendor/ntgcalls/lib -lntgcalls -lyaml-cpp -lcurl -lpthread -ldl -lz -lresolv $(MONGO_LIBS)

SRCDIR = src
INCDIR = include
BINDIR = bin
TESTDIR = tests

SOURCES = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/tinaproto/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.o,$(SOURCES))
TARGET = $(BINDIR)/tina-music-bot

TEST_TARGET = $(BINDIR)/test_full

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@ln -sf tina-music-bot $(BINDIR)/yukki-music-bot
	@echo "Build complete: $(TARGET) (C++20 with MongoDB persistence support)"

$(BINDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	@echo "Running full test suite..."
	@./$(TEST_TARGET)

$(TEST_TARGET): $(TESTDIR)/test_full.cpp $(BINDIR)/queue_manager.o $(BINDIR)/logger.o $(BINDIR)/database.o $(BINDIR)/locales.o
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lyaml-cpp -lpthread $(MONGO_LIBS)

clean:
	rm -rf $(BINDIR)

.PHONY: all test clean
