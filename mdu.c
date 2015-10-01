#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int opt_follow_links = 0;
static int opt_apparent_size = 0;
static int opt_count_total = 0;

/* prototypes these two functions because of cross recursion */
static size_t expand_directory(const char *pathname, int depth);
static size_t process_file(const char *pathname, int depth);

/* call process_file() on each file of a directory, computing the sum of the sizes */
static size_t expand_directory(const char *pathname, int depth){
  DIR *dir;
  struct dirent *dirent;
  size_t size = 0;
  char *entry_path = NULL;
  size_t dir_path_len, entry_path_len;

  /* open directory */
  dir = opendir(pathname);
  if (dir == NULL){
    perror(pathname);
    return 0;
  }

  /* compute the length of pathname */
  dir_path_len = strlen(pathname);

  /* foreach file in directory */
  while ((dirent = readdir(dir)) != NULL){

    /* skip '.' and '..' */
    if (strcmp(dirent->d_name, ".") == 0 ||
        strcmp(dirent->d_name, "..") == 0){
      continue;
    }

    /* compute the length of the current entry path */
    entry_path_len = dir_path_len + 1 + strlen(dirent->d_name); /*without '\0'*/

    /* allocate the mem for the current entry path,
      using realloc for better perf */
    entry_path = realloc(entry_path, (entry_path_len + 1) * sizeof(char));
    if (entry_path == NULL){
      perror("malloc");
      exit(EXIT_FAILURE);
    }

    /* build the current entry path */
    strcpy(entry_path, pathname);
    if (dir_path_len > 0 && entry_path[dir_path_len - 1] != '/'){
      strcat(entry_path, "/");
    }
    strcat(entry_path, dirent->d_name);

    /* recursive call to process_file() */
    size += process_file(entry_path, depth + 1);
  }

  closedir(dir);

  /* return the computed sum of size */
  return size;
}

/* compute the size of a file recursively */
static size_t process_file(const char *pathname, int depth){
  struct stat sb;
  size_t size;
  int stat_ret;

  /* stat() or lstat() the file */
  stat_ret = (opt_follow_links ? stat : lstat)(pathname, &sb);
  if (stat_ret == -1){
    perror(pathname);
    return 0;
  }

  /* get the size */
  if (opt_apparent_size){
    size = sb.st_size;
  }
  else {
    size = sb.st_blocks;
  }

  /* recursive call if file is a directory */
  if (S_ISDIR(sb.st_mode)){
    size += expand_directory(pathname, depth);
    /* show information if depth > 0 */
    if (depth > 0){
      printf("%lu\t%s\n", size, pathname);
    }
  }

  return size;
}

static size_t du_file(const char *pathname){
  size_t size = process_file(pathname, 0);
  printf("%lu\t%s\n", size, pathname);
  return size;
}

static void print_usage(const char *progname){
  printf("usage: %s [-b] [-L] [FILE]...\n", progname);
}

/* return -1 if an error is detected */
static int parse_options(int ac, char **av){

  int opt;

  while ((opt = getopt(ac, av, "bLc")) != -1){
    switch (opt){
      case 'b':
        opt_apparent_size = 1;
        break;
      case 'L':
        opt_follow_links = 1;
        break;
      case 'c':
        opt_count_total = 1;
        break;
      default:
        return -1;
    }
  }
  return 0;
}


int main(int ac, char **av){
  size_t total = 0;

  /* parse opt */
  if (parse_options(ac, av) == -1){
    print_usage(av[0]);
    exit(EXIT_FAILURE);
  }

  /* if no file provided, process current directory */
  if (optind >= ac){
    total += du_file(".");
  }
  else {
    /* process the list of files provided */
    while (optind < ac){
      total += du_file(av[optind]);
      optind += 1;
    }
  }

  /* print the total if opt 'c' has been set */
  if (opt_count_total){
    printf("total\t%lu\n", total);
  }

  return EXIT_SUCCESS;
}
