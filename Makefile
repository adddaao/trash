
CURRENT_DATE=$(shell date +"%Y-%m-%d")
MACOS_INCLUDE_DIR=include/macos
SOURCE_FILES=src/macos/trash.m src/macos/HGUtils.m src/macos/HGCLIUtils.m src/macos/fileSize.m
MANPAGE_SOURCE=docs/trash.pod

.PHONY: all docs analyze clean

all: trash

docs: trash.1

trash: $(SOURCE_FILES)
	@echo
	@echo ---- Compiling:
	@echo ======================================
	$(CC) -O2 -Wall -Wextra -I$(MACOS_INCLUDE_DIR) -Wpartial-availability -Wno-unguarded-availability -force_cpusubtype_ALL -mmacosx-version-min=10.7 -arch i386 -arch x86_64 -framework AppKit -framework ScriptingBridge -o $@ $(SOURCE_FILES)

analyze:
	@echo
	@echo ---- Analyzing:
	@echo ======================================
	clang --analyze -I$(MACOS_INCLUDE_DIR) $(SOURCE_FILES)

trash.1: $(MANPAGE_SOURCE)
	@echo
	@echo ---- Generating manpage from POD file:
	@echo ======================================
	pod2man --section=1 --center="trash" --date="$(CURRENT_DATE)" $(MANPAGE_SOURCE) > trash.1

clean:
	@echo
	@echo ---- Cleaning up:
	@echo ======================================
	-trash trash
	-trash trash.exe
	-trash trash.1
