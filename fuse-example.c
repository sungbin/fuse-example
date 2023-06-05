#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>


#include "cJSON.h"

static const char *filepath = "/file";
static const char *filename = "file";
static const char *filecontent = "I'm the content of the only file available there\n";

static int getattr_callback(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }

  if (strcmp(path, filepath) == 0) {
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(filecontent);
    return 0;
  }

  return -ENOENT;
}

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) {
  (void) offset;
  (void) fi;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  filler(buf, filename, NULL, 0);

  return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
  return 0;
}

static int read_callback(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi) {

  if (strcmp(path, filepath) == 0) {
    size_t len = strlen(filecontent);
    if (offset >= len) {
      return 0;
    }

    if (offset + size > len) {
      memcpy(buf, filecontent + offset, len - offset);
      return len - offset;
    }

    memcpy(buf, filecontent + offset, size);
    return size;
  }

  return -ENOENT;
}

static struct fuse_operations fuse_example_operations = {
  .getattr = getattr_callback,
  .open = open_callback,
  .read = read_callback,
  .readdir = readdir_callback,
};

/* Define structure and functions for parsing JSON */
typedef struct {
    char name[10];
    int inode;
} EntryItem;

typedef struct {
    int inode;
    char type[10];
    char data[100];
    EntryItem* entries;
    int entries_count;
} Entry;

cJSON* read_json_file(const char* filename);
void parse_json(cJSON* json, Entry** entries, int* entries_count);
void print_entry(const Entry* entry);

cJSON* read_json_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* file_content = (char*)malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';

    cJSON* json = cJSON_Parse(file_content);

    free(file_content);
    fclose(file);

    return json;
}

void parse_json(cJSON* json, Entry** entries, int* entries_count) {
    int array_size = cJSON_GetArraySize(json);
    *entries = (Entry*)malloc(array_size * sizeof(Entry));
    *entries_count = array_size;

    for (int i = 0; i < array_size; i++) {
        cJSON* item = cJSON_GetArrayItem(json, i);
        Entry* entry = &((*entries)[i]);

        entry->inode = cJSON_GetObjectItem(item, "inode")->valueint;
        const char* type = cJSON_GetObjectItem(item, "type")->valuestring;
        strncpy(entry->type, type, strlen(type));
	(entry->type)[strlen(type)] = 0;

        if (cJSON_HasObjectItem(item, "entries")) {
            cJSON* entries = cJSON_GetObjectItem(item, "entries");
            int entries_size = cJSON_GetArraySize(entries);
            entry->entries = (EntryItem*)malloc(entries_size * sizeof(EntryItem));
            entry->entries_count = entries_size;

            for (int j = 0; j < entries_size; j++) {
                cJSON* entry_item = cJSON_GetArrayItem(entries, j);
                EntryItem* item = &(entry->entries[j]);
                const char* name = cJSON_GetObjectItem(entry_item, "name")->valuestring;
                strncpy(item->name, name, strlen(name));
		(item->name)[strlen(name)] = 0;
                item->inode = cJSON_GetObjectItem(entry_item, "inode")->valueint;
            }
        }
	else {
	    entry->entries = NULL;
            entry->entries_count = 0;
	}

        if (cJSON_HasObjectItem(item, "data")) {
            const char* data = cJSON_GetObjectItem(item, "data")->valuestring;
            strncpy(entry->data, data, strlen(data));
            (entry->data)[strlen(data)] = 0;
        }
	else {
	    entry->data[0] = 0;
	}
    }
}

void print_entry(const Entry* entry) {
    printf("inode: %d, type: %s\n", entry->inode, entry->type);

    if (entry->entries_count > 0) {
        printf("entries:\n");
        for (int i = 0; i < entry->entries_count; i++) {
            const EntryItem* item = &(entry->entries[i]);
            printf("  name: %s, inode: %d\n", item->name, item->inode);
        }
    }

    if (strlen(entry->data) > 0) {
        printf("data: %s\n", entry->data);
    }

    printf("\n");
}

int main(int argc, char *argv[])
{

  /* parsing JSON with cJSON*/
  cJSON* json = read_json_file("input.json");
  if (json == NULL)
    return 1;

  Entry* entries = NULL;
  int entries_count = 0;

  parse_json(json, &entries, &entries_count);

  for (int i = 0; i < entries_count; i++) {
    print_entry(&entries[i]);
  }


  for (int i = 0; i < entries_count; i++) {
    if (entries[i].entries != NULL)
      free(entries[i].entries);
  }
  free(entries);

  cJSON_Delete(json);
  /* ~ parsing JSON with cJSON*/

  return fuse_main(argc, argv, &fuse_example_operations, NULL);
}
