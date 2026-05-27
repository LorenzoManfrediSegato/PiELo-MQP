INSDIR = instructions
BUILDDIR = build

CXX = g++
# -MMD -MP emit .d header-dependency files so edits to a header trigger rebuilds.
CXXFLAGS = -std=c++17 -Wall -Wextra -g -MMD -MP

EXECUTABLE = VM
# SOURCES = $(wildcard *.cpp,$(INSDIR)/*.cpp)
INSOBJECTS = $(patsubst %.o, $(BUILDDIR)/$(INSDIR)/%.o, arithmetic.o closureInstructions.o comparisonJumps.o simpleInstructions.o storeLoad.o stigmergy.o)
OBJECTS = $(patsubst %.o, $(BUILDDIR)/%.o, robotFunctions.o networking.o instructionHandler.o parser.o vm.o gc.o)  $(INSOBJECTS) 
DEBUG_PARSER = 0
DEBUG_INSTRUCTIONS = 1

ifeq ($(DEBUG_PARSER), 1)
	DEBUG_PARSER = DEBUG_PARSER
else
	DEBUG_PARSER = 
endif

ifeq ($(DEBUG_INSTRUCTIONS), 1)
	DEBUG_INSTRUCTIONS = DEBUG_INSTRUCTIONS
else
	DEBUG_INSTRUCTIONS = 
endif
DEFINES = $(patsubst %, -D __%__, $(DEBUG_PARSER) $(DEBUG_INSTRUCTIONS))

all: dir $(EXECUTABLE) router

dir:
	mkdir -p $(BUILDDIR)
	mkdir -p $(BUILDDIR)/$(INSDIR)

# $^ means "all of the prereqs" and $@ means "name of the target"
$(EXECUTABLE): $(OBJECTS) $(BUILDDIR)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

# see https://www.gnu.org/software/make/manual/html_node/Static-Usage.html for details
$(OBJECTS): $(BUILDDIR)/%.o : %.cpp %.h vm.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(DEFINES)

$(BUILDDIR)/main.o: main.cpp robotFunctions.cpp robotFunctions.h
	$(CXX) $(CXXFLAGS) -c main.cpp -o $(BUILDDIR)/main.o

router: router.cpp vm.h networking.h
	$(CXX) $(CXXFLAGS) router.cpp -o router

clean:
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/*.d $(BUILDDIR)/$(INSDIR)/*.o $(BUILDDIR)/$(INSDIR)/*.d

# Pull in auto-generated header dependencies (ignored if absent).
-include $(OBJECTS:.o=.d) $(BUILDDIR)/main.d