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
    int inode;
    char* type;
    char* data;
    struct Entry** entries;
    int num_entries;
} Entry;

Entry* create_entry() {
    Entry* entry = (Entry*)malloc(sizeof(Entry));
    entry->inode = 0;
    entry->type = NULL;
    entry->data = NULL;
    entry->entries = NULL;
    entry->num_entries = 0;
    return entry;
}

void parse_json(const char* json_string, Entry*** entries, int* num_entries) {
    cJSON* root = cJSON_Parse(json_string);
    if (root == NULL) {
        printf("Failed to parse JSON.\n");
        return;
    }

    int array_size = cJSON_GetArraySize(root);
    *entries = (Entry**)malloc(array_size * sizeof(Entry*));
    *num_entries = array_size;

    cJSON* item;
    int i = 0;
    cJSON_ArrayForEach(item, root) {
        Entry* entry = create_entry();
        (*entries)[i] = entry;

        cJSON* inode = cJSON_GetObjectItem(item, "inode");
        if (inode != NULL && cJSON_IsNumber(inode)) {
            entry->inode = inode->valueint;
        }

        cJSON* type = cJSON_GetObjectItem(item, "type");
        if (type != NULL && cJSON_IsString(type)) {
            entry->type = strdup(type->valuestring);
        }

        cJSON* data = cJSON_GetObjectItem(item, "data");
        if (data != NULL && cJSON_IsString(data)) {
            entry->data = strdup(data->valuestring);
        }

        cJSON* entries = cJSON_GetObjectItem(item, "entrues");
        if (entries != NULL && cJSON_IsArray(entries)) {
            int num_sub_entries = cJSON_GetArraySize(entries);
            entry->entries = (Entry**)malloc(num_sub_entries * sizeof(Entry*));
            entry->num_entries = num_sub_entries;

            int j = 0;
            cJSON* sub_item;
            cJSON_ArrayForEach(sub_item, entries) {
                Entry* sub_entry = create_entry();
                entry->entries[j] = sub_entry;

                cJSON* sub_inode = cJSON_GetObjectItem(sub_item, "inode");
                if (sub_inode != NULL && cJSON_IsNumber(sub_inode)) {
                    sub_entry->inode = sub_inode->valueint;
                }

                cJSON* sub_name = cJSON_GetObjectItem(sub_item, "name");
                if (sub_name != NULL && cJSON_IsString(sub_name)) {
                    // ignore attribute 'name'
                }

                j++;
            }
        }

        i++;
    }

    cJSON_Delete(root);
}

void parse_json_file(const char* file_path, Entry*** entries, int* num_entries) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        printf("Failed to open file: %s\n", file_path);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json_string = (char*)    malloc(file_size + 1);
    fread(json_string, file_size, 1, file);
    json_string[file_size] = '\0';

    fclose(file);

    parse_json(json_string, entries, num_entries);

    free(json_string);
}

void print_entry(Entry* entry, int indent) {
    if (entry == NULL) {
        return;
    }

    printf("%*sInode: %d\n", indent, "", entry->inode);
    printf("%*sType: %s\n", indent, "", entry->type);

    if (entry->type != NULL && strcmp(entry->type, "reg") == 0) {
        printf("%*sData: %s\n", indent, "", entry->data);
    }

    if (entry->entries != NULL) {
        printf("%*sEntries:\n", indent, "");
        for (int i = 0; i < entry->num_entries; i++) {
            print_entry(entry->entries[i], indent + 2);
        }
    }
}



int main(int argc, char *argv[])
{

  /* parsing JSON with cJSON*/
  Entry** entries;
  int num_entries;

  parse_json_file("input.json", &entries, &num_entries);

  for (int i = 0; i < num_entries; i++) {
       print_entry(entries[i], 0);
  }

  for (int i = 0; i < num_entries; i++) {
      free(entries[i]->type);
      free(entries[i]->data);
      free(entries[i]->entries);
      free(entries[i]);
  }
  free(entries);
  /* ~ parsing JSON with cJSON*/

  return fuse_main(argc, argv, &fuse_example_operations, NULL);
}
