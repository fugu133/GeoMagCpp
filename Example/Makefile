CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++14 -O2 -I../

all: geomag

geomag: CalcGeoMag.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f geomag