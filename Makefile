CXX = g++
CXXFLAGS = -std=c++17 -I./imgui -I./imgui/backends -Wall -Wextra -O2
LDFLAGS = -lglfw -lGL -lsqlite3 -lpthread

SOURCES = main.cpp \
          imgui/imgui.cpp \
          imgui/imgui_demo.cpp \
          imgui/imgui_draw.cpp \
          imgui/imgui_tables.cpp \
          imgui/imgui_widgets.cpp \
          imgui/backends/imgui_impl_glfw.cpp \
          imgui/backends/imgui_impl_opengl3.cpp

OBJECTS = $(SOURCES:.cpp=.o)

EXEC = league_gui

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXEC)
