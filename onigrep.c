#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <oniguruma.h>

typedef unsigned char   UCHAR;
typedef unsigned char *UCHARP;

void matcher(UCHARP regexp, UCHARP beg, UCHARP buf, UCHARP end);
void grep(char *regexp, int fd, char *name);

int main(int argc, char* argv[]) {
  int opt,len,i, fd;

  char buf[1024], *regexp = NULL;

  if (argc < 3) {
    fprintf(stderr, "usage: grep (regexp|-f regexp-file) file+");
    exit(1);
  } else {
    while ((opt=getopt(argc, argv, "f:")) != -1) {
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
  UCHARP buf, end, beg;
  off_t size;
  struct stat sb;

  if (fstat(fd, &sb)) {
    fprintf(stderr, "can't fstat %s\n", name);
    exit(0);
  }

  size = sb.st_size;
  file_mmap = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, (off_t)0);

  if (file_mmap == (caddr_t)-1) {
    fprintf(stderr, "can't mmap %s\n", name);
    exit(0);
  }

  beg = buf = (UCHARP) file_mmap;
  end = beg + size - 1;

  matcher((UCHARP)regexp, beg, beg, end);

  munmap(file_mmap, size);
  return;
}

void matcher(UCHARP regexp, UCHARP beg, UCHARP buf, UCHARP end) {
  regex_t *reg;
  OnigErrorInfo einfo;
  OnigRegion *region;
  int r, len = strlen((char *)regexp);

  /* modify regexp for match to 1-line.
     ex: (a|b)*c -> ^.*(a|b)*c.*$
   */

  UCHARP regexp_ = (UCHARP)malloc(sizeof(UCHAR*)*strlen((char *)regexp)+6);
  strcpy((char *)regexp_+3, (char *)regexp);
  regexp_[0] = '^';
  regexp_[1] = '.';
  regexp_[2] = '*';
  regexp_[len+3] = '.';
  regexp_[len+4] = '*';
  regexp_[len+5] = '$';
  regexp_[len+6] = '\0';

  r = onig_new(&reg, regexp_, regexp_ + strlen((char *)regexp_),
        ONIG_OPTION_DEFAULT, ONIG_ENCODING_ASCII, ONIG_SYNTAX_DEFAULT, &einfo);
  if (r != ONIG_NORMAL) {
    UCHAR s[ONIG_MAX_ERROR_MESSAGE_LEN];
    onig_error_code_to_str(s, r, &einfo);
    fprintf(stderr, "ERROR: %s\n", s);
    return;
  }

  region = onig_region_new();
  buf = beg;

  for (;;) {
    r = onig_search(reg, buf, end, buf, end, region, ONIG_OPTION_NONE);

    if (r >= 0) {
      fwrite(buf+region->beg[0], sizeof(char),
             (region->end[0] - region->beg[0] + 1), stdout);
      buf += region->end[0]+1;
    } else if (r == ONIG_MISMATCH) {
      break;
    }
    else {
      UCHAR s[ONIG_MAX_ERROR_MESSAGE_LEN];
      onig_error_code_to_str(s, r);
      fprintf(stderr, "ERROR: %s\n", s);
      return;
    }
  }

  onig_region_free(region, 1 /* 1:free self, 0:free contents only */);
  onig_free(reg);
  onig_end();

  return;
}
