#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "hashmap.h"

#define HASH_MAP_INIT_SIZE      16  //ハッシュテーブルの初期サイズ
#define HASH_MAP_MAX_CAPACITY   200 //使用率をこれ以下に抑える
#define HASH_MAP_GROW_FACTOR    2   //リハッシュ時に何倍にするか

//ハッシュエントリ
typedef struct hash_entry hash_entry_t;
typedef struct hash_entry {
    hash_entry_t *next;
    void *data;             //ハッシュデータ（任意のポインタ）
    char key[];             //ハッシュキー（文字列）
} hash_entry_t;

//ハッシュマップ本体
typedef struct hash_map {
    int num;                //データ数
    int capacity;           //配列の最大数
    hash_entry_t **buckets; //配列
} hash_map_t;

static uint32_t fnv1a_hash(const char *str);
static uint32_t fnv1_hash(const char *str);
static uint32_t dbg_hash(const char *str);
static uint32_t calc_hash(const char *str);
static void rehash(hash_map_t *hash_map);
static int match(const char *key1, const char *key2);
static void insert_entry(hash_map_t *hash_map, hash_entry_t *entry, int idx);

//ハッシュマップを作成する。
hash_map_t *new_hash_map(void) {
    hash_map_t *hash_map = calloc(1, sizeof(hash_map_t));
    hash_map->capacity = HASH_MAP_INIT_SIZE;
    hash_map->buckets = calloc(HASH_MAP_INIT_SIZE, sizeof(hash_entry_t*));
    return hash_map;
}

//ハッシュマップをフリーする。
void free_hash_map(hash_map_t *hash_map) {
    if (hash_map) {
        for (int i=0; i<hash_map->capacity; i++) {
            hash_entry_t *entry = hash_map->buckets[i];
            while (entry) {
                hash_entry_t *next = entry->next;
                free(entry);
                entry = next;
            }
        }
        free(hash_map->buckets);
    }
    free(hash_map);
}

//https://jonosuke.hatenadiary.org/entry/20100406/p1
//http://www.isthe.com/chongo/tech/comp/fnv/index.html
#define OFFSET_BASIS32      2166136261  //2^24 + 2^8 + 0x93
#define FNV_PRIME32         16777619
static uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = OFFSET_BASIS32;
    for (const char *p=str; *p; p++) {
        hash ^= *p;
        hash *= FNV_PRIME32;
    }
    return hash;
}
static uint32_t fnv1_hash(const char *str) {
    uint32_t hash = OFFSET_BASIS32;
    for (const char *p=str; *p; p++) {
        hash *= FNV_PRIME32;
        hash ^= *p;
    }
    return hash;
}
static uint32_t dbg_hash(const char *str) {
    uint32_t hash = 0;
    for (const char *p=str; *p; p++) {
        hash *= 3;
        hash += *p;
    }
    return hash;
}
hash_map_func_type_t hash_map_func = HASH_MAP_FUNC_FNV_1A;
static uint32_t calc_hash(const char *str) {
    switch (hash_map_func) {
        case HASH_MAP_FUNC_FNV_1A:
            return fnv1a_hash(str);
        case HASH_MAP_FUNC_FNV_1:
            return fnv1_hash(str);
        case HASH_MAP_FUNC_DBG:
            return dbg_hash(str);
        default:
            assert(0);
    }
}

//リハッシュ
static void rehash(hash_map_t *hash_map) {
    hash_map_t new_map = {};
    //dump_hash_map(__func__, hash_map, 0);

    //サイズを拡張した新しいハッシュマップを作成する
    new_map.capacity = hash_map->capacity * HASH_MAP_GROW_FACTOR;
    new_map.buckets = calloc(new_map.capacity, sizeof(hash_entry_t*));

    //すべてのエントリをコピーする
    for (int i=0; i<hash_map->capacity; i++) {
        hash_entry_t *entry = hash_map->buckets[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            int idx = calc_hash(entry->key) % new_map.capacity;
            insert_entry(&new_map, entry, idx);
            entry = next;
        }
    }
    assert(hash_map->num==new_map.num);
    free(hash_map->buckets);
    *hash_map = new_map;
}

//キーの一致チェック
static int match(const char *key1, const char *key2) {
    return strcmp(key1, key2)==0;
}

//エントリをリストに挿入
static void insert_entry(hash_map_t *hash_map, hash_entry_t *entry, int idx) {
    hash_entry_t *top_entry = hash_map->buckets[idx];
    entry->next = top_entry;
    hash_map->buckets[idx] = entry;
    hash_map->num++;
}

//データ書き込み
//すでにデータが存在する場合は上書きし0を返す。新規データ時は1を返す。
//キーにNULLは指定できない。dataにNULLを指定できる。
int put_hash_map(hash_map_t *hash_map, const char *key, void *data) {
    assert(hash_map);
    assert(key);
    int idx = calc_hash(key) % hash_map->capacity;
    hash_entry_t *entry = hash_map->buckets[idx];
    hash_entry_t **parent = &hash_map->buckets[idx];
    while (entry) {
        if (match(entry->key, key)) {   //データが存在するので上書き
            *parent = entry = realloc(entry, sizeof(hash_entry_t)+strlen(key)+1);
            strcpy(entry->key, key);
            entry->data = data;
            return 0;
        }
        parent = &entry->next;
        entry = entry->next;
    }

    //データ追加
    if ((hash_map->num * 100) / hash_map->capacity > HASH_MAP_MAX_CAPACITY ) {
        rehash(hash_map);
        idx = calc_hash(key) % hash_map->capacity;
    }
    entry = calloc(1, sizeof(hash_entry_t)+strlen(key)+1);
    strcpy(entry->key, key);
    entry->data = data;
    insert_entry(hash_map, entry, idx);
    return 1;
}

//キーに対応するデータの取得
//存在すればdataに値を設定して1を返す。
//存在しなければ0を返す。dataにNULLを指定できる。
int get_hash_map(hash_map_t *hash_map, const char *key, void **data) {
    assert(hash_map);
    assert(key);
    int idx = calc_hash(key) % hash_map->capacity;
    hash_entry_t *entry = hash_map->buckets[idx];
    while (entry) {
        if (match(entry->key, key)) {
            if (data) *data = entry->data;
            return 1;
        }
        entry = entry->next;
    }
    return 0;
}

//データの削除
//キーに対応するデータを削除して1を返す。
//データが存在しない場合は0を返す。
int del_hash_map(hash_map_t *hash_map, const char *key) {
    assert(hash_map);
    assert(key);
    int idx = calc_hash(key) % hash_map->capacity;
    hash_entry_t *entry   =  hash_map->buckets[idx];
    hash_entry_t **parent = &hash_map->buckets[idx];
    while (entry) {
        if (match(entry->key, key)) {
            *parent = entry->next;
            free(entry);
            hash_map->num--;
            return 1;
        }
        parent = &entry->next;
        entry = entry->next;
    }
    return 0;
}

//ハッシュマップのデータ数
int num_hash_map(hash_map_t *hash_map) {
    return hash_map->num;
}

//イテレータ
typedef struct iterator {
    int             next_idx;
    hash_entry_t    *next_entry;
    hash_map_t      *hash_map;
} iterator_t;

//ハッシュマップのイテレータを生成する。
iterator_t *iterate_hash_map(hash_map_t *hash_map) {
    assert(hash_map);
    iterator_t *iterator = calloc(1, sizeof(iterator_t));
    iterator->hash_map = hash_map;
    return iterator;
}

//次のデータをkey,dataに設定して1を返す。key、dataにNULL指定可能。
//次のデータがない場合は0を返す。
//イテレートする順番はランダム。
int next_iterate(iterator_t* iterator, char **key, void **data) {
    assert(iterator);
    hash_map_t   *hash_map   = iterator->hash_map;
    hash_entry_t *hash_entry = iterator->next_entry;
    for (; iterator->next_idx < hash_map->capacity; iterator->next_idx++) {
        if (hash_entry==NULL)
            hash_entry = hash_map->buckets[iterator->next_idx];
        if (hash_entry) {
            if (key)  *key  = hash_entry->key;
            if (data) *data = hash_entry->data;
            iterator->next_entry = hash_entry->next;
            if (iterator->next_entry==NULL) iterator->next_idx++;
            return 1;
        }
    }
    return 0;
}

//ハッシュマップのイテレータを解放する。
void end_iterate(iterator_t* iterator) {
    free(iterator);
}

//ハッシュマップをダンプする
//level=0: 基本情報のみ
//level=1: 有効なキーすべて
void dump_hash_map(const char *str, hash_map_t *hash_map, int level) {
    int n_col = 0;      //衝突の発生回数
    int n_used = 0;     //配列の使用数
    int max_chain = 0;  //チエインの最大長
    for (int i=0; i<hash_map->capacity; i++) {
        hash_entry_t *entry = hash_map->buckets[i];
        if (!entry) continue;
        n_used++;
        entry = entry->next;
        int n_chain = 1;
        while (entry) {
            n_col++;
            n_chain++;
            entry = entry->next;
        }
        if (n_chain > max_chain) max_chain = n_chain;
    }
    fprintf(stderr, "= %s: num=%d,\tcapacity=%d(%d%%),\tused=%d%%,\tcollision=%d%%,\tmax_chain=%d\n", 
        str, hash_map->num, hash_map->capacity, hash_map->num*100/hash_map->capacity,
        n_used*100/hash_map->capacity, n_col*100/hash_map->capacity, max_chain);
    if (level>0) {
        for (int i=0; i<hash_map->capacity; i++) {
            hash_entry_t *entry = hash_map->buckets[i];
            if (!entry) continue;
            fprintf(stderr, "%03d: \"%s\", %p\n", i, entry->key, entry->data);
            entry = entry->next;
            while (entry) {
                fprintf(stderr, "   : \"%s\", %p\n", entry->key, entry->data);
                entry = entry->next;
            }
        }
    }
}
