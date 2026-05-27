BUILDDIR := build
OBJECTS := $(patsubst %.o, $(BUILDDIR)/%.o, tokenizer.o parser.o codegen.o)
EXECUTABLE = parser

CXX = g++
# -MMD -MP emit .d header-dependency files so edits to a header trigger rebuilds.
CXXFLAGS = -std=c++17 -Wall -Wextra -g -MMD -MP

all: $(EXECUTABLE)



# $^ means "all of the prereqs" and $@ means "name of the target"
$(EXECUTABLE): $(OBJECTS) $(BUILDDIR)/main.o | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILDDIR)/main.o:	main.cpp tokenizer.h parser.h codegen.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/main.o -c main.cpp

$(BUILDDIR)/tokenizer.o: tokenizer.cpp tokenizer.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/tokenizer.o -c tokenizer.cpp

$(BUILDDIR)/parser.o: parser.cpp parser.h tokenizer.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/parser.o -c parser.cpp

$(BUILDDIR)/codegen.o: codegen.cpp codegen.h parser.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/codegen.o -c codegen.cpp

$(BUILDDIR):
	mkdir $(BUILDDIR)

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/*.d parser

# Pull in auto-generated header dependencies (ignored if absent).
-include $(OBJECTS:.o=.d) $(BUILDDIR)/main.d