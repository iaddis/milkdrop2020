


#
# Makefile for building with emscripten
# This is intended to be run from the root dir:
# make -C ../..
#

CC         = emcc

EMSDK_ROOT = ../emsdk_portable
EMSDK      = $(EMSDK_ROOT)/emsdk
EMENV      = $(EMSDK_ROOT)/emsdk_set_env.sh

CONFIG     ?= release
BUILD_DIR   = build/emscripten/$(CONFIG)
OUTPUT_DIR  = $(BUILD_DIR)/bin
OUTPUT_HTML = $(OUTPUT_DIR)/index.html
EMRUN       = emrun
BROWSER    ?= chrome


HLSL2GLSL_ROOT := external/hlsl2glslfork/hlslang

INCLUDE_DIRS += src
INCLUDE_DIRS += src/script
INCLUDE_DIRS += src/script/mdp-eel2

SOURCES := $(shell find src -name '*.cpp')
SOURCES += $(shell find src -name '*.c' )
SOURCES += $(shell find app/emscripten -name '*.cpp')

# include ImGui
SOURCES += external/imgui/imgui.cpp
SOURCES += external/imgui/imgui_widgets.cpp
SOURCES += external/imgui/imgui_impl_sdl.cpp
SOURCES += external/imgui/imgui_draw.cpp
SOURCES += external/imgui/imgui_demo.cpp

SOURCES += $(shell find $(HLSL2GLSL_ROOT)/GLSLCodeGen -name '*.cpp')
SOURCES += $(shell find $(HLSL2GLSL_ROOT)/MachineIndependent -name '*.cpp')
SOURCES += $(shell find $(HLSL2GLSL_ROOT)/OSDependent/Emscripten -name '*.cpp')


SOURCES_JS = $(shell find src -name '*.js')

INCLUDE_DIRS += $(HLSL2GLSL_ROOT)
INCLUDE_DIRS += $(HLSL2GLSL_ROOT)/hlslang
INCLUDE_DIRS += $(HLSL2GLSL_ROOT)/Include
INCLUDE_DIRS += $(HLSL2GLSL_ROOT)/MachineIndependent
INCLUDE_DIRS += $(HLSL2GLSL_ROOT)/GLSLCodeGen
INCLUDE_DIRS += $(HLSL2GLSL_ROOT)/OSDependent/Emscripten


OBJS       := $(SOURCES:%=$(BUILD_DIR)/%.o)
DEPS       := $(SOURCES:%=$(BUILD_DIR)/%.d)
#DATA_FILES := $(shell find assets)
SHELL_HTML ?= app/emscripten/emscripten_shell.html


#
# compile flags
#

ifeq ($(CONFIG),debug)
CC_FLAGS += -g
CC_FLAGS += -O1
CC_FLAGS += -DDEBUG=1
else ifeq ($(CONFIG),profile)
CC_FLAGS += -O2
CC_FLAGS += --profiling 
CC_FLAGS += --tracing
CC_FLAGS += --cpuprofiler
CC_FLAGS += -DPROFILE=1
else ifeq ($(CONFIG),release)
CC_FLAGS += -O2
CC_FLAGS += -DRELEASE=1
else
error invalid configuration $(CONFIG) (must be debug,profile,release)
endif

CC_FLAGS += -DBUILD_CONFIG=$(CONFIG)
CC_FLAGS += -Wno-deprecated-register
CC_FLAGS += -s USE_SDL=2


#
# linker flags
#

LD_FLAGS += --preload-file assets 
LD_FLAGS += --use-preload-plugins 
LD_FLAGS += --emrun 
LD_FLAGS += -s TOTAL_MEMORY=536870912
LD_FLAGS += -s DEMANGLE_SUPPORT=1

ifneq ($(CONFIG),profile)
LD_FLAGS += --shell-file $(SHELL_HTML)
endif
LD_FLAGS += -s WASM=1
LD_FLAGS += -s ASSERTIONS=1
LD_FLAGS += -s USE_WEBGL2=1
#LD_FLAGS += -s "BINARYEN_TRAP_MODE='clamp'"
LD_FLAGS += -mnontrapping-fptoint

# LD_FLAGS += -s FULL_ES3=1
LD_FLAGS += -s USE_SDL=2
LD_FLAGS +=  -lopenal
LD_FLAGS += -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=1

LD_FLAGS += $(addprefix --post-js , $(SOURCES_JS) )

#
#

.PHONY: default
default: $(OUTPUT_HTML)

.PHONY: run
run: $(OUTPUT_HTML)
	$(EMRUN) --serve_after_close --browser $(BROWSER) $(OUTPUT_HTML)

.PHONY: serve
serve: $(OUTPUT_HTML)
	open http://localhost:6931/ & 
	$(EMRUN) --no_browser $(OUTPUT_HTML)

test:
	echo $(addprefix --post-js , $(SOURCES_JS) )

.PHONY: clean
clean:
	rm -Rf $(BUILD_DIR)

.PHONY: emsdk
emsdk:
	cd .. ; curl https://s3.amazonaws.com/mozilla-games/emscripten/releases/emsdk-portable.tar.gz | tar -zxvp
	$(EMSDK) update
	$(EMSDK) install latest
	$(EMSDK) activate latest


$(OUTPUT_HTML): $(OBJS) $(SHELL_HTML) $(SOURCES_JS) $(MAKEFILE_LIST)
	@mkdir -p $(@D)
	@echo Linking $@
	@$(CC) $(OBJS) -o $@ $(CC_FLAGS) $(LD_FLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	@echo Compiling $<
	@$(CC) -c $< -o $@ -MMD $(CC_FLAGS) $(INCLUDE_DIRS:%=-I%) 

$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	@echo Compiling $<
	@$(CC) -c $< -o $@ -MMD -std=c++11 $(CC_FLAGS) $(INCLUDE_DIRS:%=-I%) 

.PHONY: deploy
deploy:
	aws s3 cp $(OUTPUT_DIR)  s3://soundproof-vancouver/mdp/ --recursive


-include $(DEPS)
