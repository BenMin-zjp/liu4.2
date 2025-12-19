# 使用 MinGW 的 gcc
CC      = gcc

# 头文件路径：
#   include            -> 你自己的 .h
#   C:/SDL2/include    -> SDL2 / SDL2_ttf 的头文件（如果你装在别的位置，自己改路径）
CFLAGS  = -Wall -O2 -Iinclude -IC:/SDL2/include

# 链接参数：
#   -LC:/SDL2/lib      -> SDL2 / SDL2_ttf 的库文件目录
#   -lmingw32          -> MinGW 的启动库
#   -lSDL2main -lSDL2  -> SDL2 主库
#   -lSDL2_ttf         -> 字体库
#   -mwindows          -> 窗口程序（不弹控制台窗口）
LDFLAGS = -LC:/SDL2/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -mwindows

# 源码和目标文件目录
SRCDIR  = src
OBJDIR  = build

# 所有要编译的 .c 文件
SOURCES = \
	$(SRCDIR)/main.c   \
	$(SRCDIR)/game.c   \
	$(SRCDIR)/gui.c    \
	$(SRCDIR)/ai.c     \
	$(SRCDIR)/fileio.c \
	$(SRCDIR)/utils.c

# 把 src/xxx.c 映射成 build/xxx.o
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# 生成的可执行文件名
TARGET  = six.exe

# 默认目标：运行 mingw32-make 时会执行
all: $(TARGET)

# 确保 build 目录存在
$(OBJDIR):
	mkdir $(OBJDIR)

# 通用的 .c -> .o 规则
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 最终链接成 six.exe
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# 清理
clean:
	-del $(OBJDIR)\*.o 2>nul
	-del $(TARGET) 2>nul
