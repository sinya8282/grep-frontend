#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <re2/re2.h>
#include <re2/stringpiece.h>
#include <limits.h>

using namespace std;
using namespace re2;

typedef unsigned char   UCHAR;
typedef unsigned char *UCHARP;
int count_line = -1;

void grep(char *regexp, int fd, char *name);
void matcher(RE2 &re, UCHARP beg, UCHARP end);
UCHARP get_line_beg(UCHARP p, UCHARP beg);

int main(int argc, char* argv[]) {
  int opt, i, fd;

  char buf[1024], *regexp = NULL;

  if (argc < 3) {
    fprintf(stderr, "usage: gre2p [-c] (regexp|-f regexp-file) file+");
    exit(1);
  } else {
    while ((opt=getopt(argc, argv, "f:c")) != -1) {
      switch (opt) {
      case 'f':
        {
          FILE *fp = fopen(optarg, "r");
          fgets(buf, sizeof(buf), fp);
          regexp = buf;
          if (buf[strlen(buf)-1] == '\n') {
            buf[strlen(buf)-1] = '\0';
          }
          fclose(fp);
          optind--;
        }
        break;
	  case 'c':
		count_line = 0;
		break;
      default:
        fprintf(stderr, "usage: grep (regexp|-f regexp-file) file+");
        exit(1);
      }
    }

    if (regexp == NULL) {
      if (optind > argc) {
        fprintf(stderr, "usage: grep (regexp|-f regexp-file) file+");
        exit(1);
      }
     regexp = argv[optind];
    }

    for (i = optind+1; i < argc; i++) {
      fd = open(argv[i], O_RDONLY, 0666);
      if (fd == 0) {
        fprintf(stderr, "can't open %s:", argv[i]);
        continue;
      }
      grep(regexp, fd, argc > 3 ? argv[i] : NULL);
      close(fd);
    }
  }

  return 0;
}

void grep(char *regexp, int fd, char *name) {
  caddr_t file_mmap;
  UCHARP end, beg, end_;
  off_t size;
  struct stat sb;

  if (fstat(fd, &sb)) {
    fprintf(stderr, "can't fstat %s\n", name);
    exit(0);
  }

  size = sb.st_size;
  file_mmap = (caddr_t)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, (off_t)0);

  if (file_mmap == (caddr_t)-1) {
    fprintf(stderr, "can't mmap %s\n", name);
    exit(0);
  }

  beg = (UCHARP) file_mmap;
  end = beg + size - 1;

  string regex = regexp;

  RE2 re("(.*"+regex+".*)");
  re.ok();

  for(;;) {
    /* divide string into INT_MAX length.
       (RE2::StringPiece._length is int)
    */
    end_ = (beg+INT_MAX);
    if (end_ > end) {
      end_ = end;
    } else {
      end_ = get_line_beg(end_, beg)-1;
      if ((end_+1) == beg) {
        fprintf(stderr, "too long line.");
        exit(1);
      }
    }
    matcher(re, beg, end_);
    if (end_ == end) break;
    beg = end_ + 1;
  }

  munmap(file_mmap, size);
  return;
}

void matcher(RE2 &re, UCHARP beg, UCHARP end) {
  string contents = string((const char *)beg, (end - beg));
  StringPiece input(contents);
  string word;
  while (RE2::FindAndConsume(&input, re, &word)) {
	if (count_line == -1) {
	  cout << word << endl;
	} else {
	  count_line++;
	}
  }
  if (count_line != -1) cout << count_line << endl;
  return;
}

UCHARP get_line_beg(UCHARP p, UCHARP beg) {
  while(p > beg) {
    if ((*--p) == '\n') return p+1;
  }
  return beg;
}
