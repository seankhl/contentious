#### PROJECT SETTINGS ####

# The name of the executable to be created
BIN_NAME := vector-tests

# Compiler used
CXX_NAME = g++
ifeq ($(CXX_NAME),clang++)
	CXX_VER = 3.9
else
	CXX_VER = 5
endif
CXX = $(CXX_NAME)-$(CXX_VER)

# Extension of source files used in the project
SRC_EXT = cc
# Path to the source directory, relative to the makefile
SRC_PATH = ./bp_vector
# Path to directory where libs will be built
LIB_PATH = ./lib
# Path to test source directory
TEST_PATH = ./tests
# Space-separated pkg-config libraries used by this project
LIBS =

ifeq ($(CXX_NAME),clang++)
	OPENMP = -fopenmp=libiomp5
	CXX_OG = -O0
else
	OPENMP = -fopenmp
	CXX_OG = -Og
endif

BOOST_PATH = /home/sean/Documents/software/modular-boost/stage/lib
#BOOST_PATH = ./bp_vector/boost-deps/stage/lib

# General compiler flags
COMPILE_FLAGS = -std=c++1z -Wall -Wextra -march=native -mtune=generic	\
				$(OPENMP) -mavx
# Additional release-specific flags
RCOMPILE_FLAGS = -DRELEASE -O3 -DNDEBUG
# Additional debug-specific flags
DCOMPILE_FLAGS = -DDEBUG $(CXX_OG) -g
# Add additional include paths
NEUROTIC_COMPILE_FLAGS = -pedantic -Wcast-align -Wcast-qual				\
	-Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self	\
	-Wlogical-op -Wmissing-include-dirs -Wnoexcept -Woverloaded-virtual	\
	-Wredundant-decls -Wshadow -Wsign-promo -Wstrict-null-sentinel		\
	-Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused

INCLUDES = -isystem /home/sean/Documents/software/modular-boost
# General linker settings
LINK_FLAGS = $(OPENMP) -mavx -lpthread						\
			 -Wl,-rpath=$(BOOST_PATH) -L$(BOOST_PATH)		\
			 -lboost_thread -lboost_context -lboost_system	\
			 # -lzmq -lprotobuf
# Additional release-specific linker settings
RLINK_FLAGS =
# Additional debug-specific linker settings
DLINK_FLAGS =
# Destination directory, like a jail or mounted system
DESTDIR = /
# Install path (bin/ is appended automatically)
INSTALL_PREFIX = /usr/local
#### END PROJECT SETTINGS ####

# Generally should not need to edit below this line

# Function used to check variables. Use on the command line:
# 	make print-VARNAME
# Useful for debugging and adding features
print-%: ; @echo $*=$($*)

# Shell used in this makefile
# bash is used for 'echo -en'
SHELL = /bin/bash

# Clear built-in rules
.SUFFIXES:

# Programs for installation
INSTALL = install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

# Append pkg-config specific libraries if need be
ifneq ($(LIBS),)
	COMPILE_FLAGS += $(shell pkg-config --cflags $(LIBS))
	LINK_FLAGS += $(shell pkg-config --libs $(LIBS))
endif

# Verbose option, to output compile and link commands
export V := true
export CMD_PREFIX := @
ifeq ($(V),true)
	CMD_PREFIX :=
endif

# Debug by default, or use make DEBUG=0
DEBUG ?= 1
ifeq ($(DEBUG), 1)
	export CXXFLAGS = $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
	export LDFLAGS = $(LINK_FLAGS) $(DLINK_FLAGS)
	export BUILD_PATH = build/debug
	export BIN_PATH = bin/debug
else
	export CXXFLAGS = $(COMPILE_FLAGS) $(RCOMPILE_FLAGS)
	export LDFLAGS = $(LINK_FLAGS) $(RLINK_FLAGS)
	export BUILD_PATH = build/release
	export BIN_PATH = bin/release
endif

ifeq ($(TCMALLOC), 1)
	CXXFLAGS += -fno-builtin-malloc -fno-builtin-calloc	\
                -fno-builtin-realloc -fno-builtin-free
	LDFLAGS += -ltcmalloc
endif
ifeq ($(PROFILE), 1)
	LDFLAGS += -lprofiler
endif

# Build and output paths
install: export BIN_PATH := bin/release
test: export SRC_PATH := tests

rwildcard = $(foreach d, $(wildcard $1*), $(call rwildcard,$d/,$2) \
				$(filter $(subst *,%,$2), $d))

# Find all source files in the source directory, sorted by most
# recently modified
SOURCES = $(shell find $(SRC_PATH)/ -not -path "$(SRC_PATH)/boost-deps/*"		\
		  							-name '*.$(SRC_EXT)' -printf '%T@\t%p\n' 	\
				| sort -k 1nr | cut -f2-)
# fallback in case the above fails
ifeq ($(SOURCES),)
	SOURCES := $(call rwildcard, $(SRC_PATH)/, *.$(SRC_EXT))
endif
# Set the object file names, with the source directory stripped
# from the path, and the build path prepended in its place
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
# Set the dependency files that will be used to add header dependencies
DEPS = $(OBJECTS:.o=.d)

# Same stuff for tests
TESTSOURCES = $(shell find $(TEST_PATH)/ 								\
		  					-name '*.$(SRC_EXT)' -printf '%T@\t%p\n'	\
				| sort -k 1nr | cut -f2-)
ifeq ($(TESTSOURCES),)
	SOURCES := $(call rwildcard, $(TEST_PATH)/, *.$(SRC_EXT))
endif
TESTOBJECTS = $(TESTSOURCES:$(TEST_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
TESTDEPS = $(TESTOBJECTS:.o=.d)

.PHONY: all
all:
	@$(MAKE) lib --no-print-directory
	@$(MAKE) test --no-print-directory

# Create library without executable
.PHONY: lib
lib:
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(LIB_PATH)
	@$(MAKE) buildlib --no-print-directory

.PHONY: test
test:
	@mkdir -p $(dir $(TESTOBJECTS))
	@mkdir -p $(BIN_PATH)
	@$(MAKE) buildexec --no-print-directory

# Installs to the set path
# TODO: add headers and libs
.PHONY: install
install:
	@echo "Installing to $(DESTDIR)$(INSTALL_PREFIX)/bin"
	@$(INSTALL_PROGRAM) $(BIN_PATH)/$(BIN_NAME) $(DESTDIR)$(INSTALL_PREFIX)/bin

# Uninstalls the program
# TODO: add headers and libs
.PHONY: uninstall
uninstall:
	@echo "Removing $(DESTDIR)$(INSTALL_PREFIX)/bin/$(BIN_NAME)"
	@$(RM) $(DESTDIR)$(INSTALL_PREFIX)/bin/$(BIN_NAME)

# Removes all build files
.PHONY: clean
clean:
	@echo "Deleting $(BIN_NAME) symlink"
	$(CMD_PREFIX)$(RM) $(BIN_NAME)
	@echo "Deleting directories"
	$(CMD_PREFIX)$(RM) -r build
	$(CMD_PREFIX)$(RM) -r lib
	$(CMD_PREFIX)$(RM) -r bin

# Compiles the protocol buffer files
.PHONY: proto
proto:
	protoc --cpp_out=. repeated_double.proto

# Deletes the protocol buffer compiled files
.PHONY: clean-proto
clean-proto:
	@echo "Deleting generated protocol buffer files"
	@$(RM) *.pb.cc
	@$(RM) *.pb.h


.PHONY: buildexec
buildexec: $(BIN_PATH)/$(BIN_NAME)

# Add dependency files, if they exist
-include $(TESTDEPS)

# Link the executable
$(BIN_PATH)/$(BIN_NAME): $(TESTOBJECTS)
	$(CMD_PREFIX)$(CXX) $(TESTOBJECTS) -L$(LIB_PATH) -lcontentious $(LDFLAGS) -o $@
	@$(RM) $(BIN_NAME)
	@ln -s $(BIN_PATH)/$(BIN_NAME) $(BIN_NAME)

$(BUILD_PATH)/%.o: $(TEST_PATH)/%.$(SRC_EXT)
	$(CMD_PREFIX)$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@


.PHONY: buildlib
buildlib: $(OBJECTS)
	ar -rsv $(LIB_PATH)/libcontentious.a $(OBJECTS)

# Add dependency files, if they exist
-include $(DEPS)

# Source file rules
# After the first compilation they will be joined with the rules from the
# dependency files to provide header dependencies
$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CMD_PREFIX)$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

