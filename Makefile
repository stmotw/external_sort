OUT_DIR := bin
SORT := $(OUT_DIR)/external_sort

LDFLAGS := -pthread
CFLAGS := -Wall -Wpedantic -Wextra -Wno-unused-parameter -O3 -std=c++1y

CPP := g++
OBJ_DIR := .obj

SRC_SORT := sort/main.cpp sort/ExternalSort.cpp

OBJS_SORT := $(addprefix $(OBJ_DIR)/,$(subst .cpp,.o,$(SRC_SORT)))

all: $(SORT)

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(SORT)

.PHONY: all clean

$(SORT): $(OUT_DIR) $(OBJS_SORT)
	@echo "LD $(SORT)"
	@$(CPP) -o $(SORT) $(OBJS_SORT) $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p `dirname $(OBJ_DIR)/$*.o`
	@$(CPP) $(CFLAGS) -c $*.cpp -o $(OBJ_DIR)/$*.o $(LDFLAGS)

$(OUT_DIR):
	mkdir $(OUT_DIR)
