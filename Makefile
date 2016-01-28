

mycode: mycode.cpp glad.c
	g++  -o myout mycode.cpp glad.c -lGL -lglfw -lftgl -lSOIL -ldl -lao -lmpg123 -I/usr/include -I/usr/local/include  -I/usr/local/include/freetype2 -L/usr/local/lib

clean:
	rm myout
