//ハッシュマップ（チェイン法）
//
//## 機能
//- キー：文字列、データ：任意のポインタ
//- 追加・削除・検索
//- 1エントリ当たり1回のmalloc（キーの保存）
//- ハッシュ関数は[FNV-1aまたはFNV-1](http://www.isthe.com/chongo/tech/comp/fnv/index.html)
//
//## 参考
//- https://jonosuke.hatenadiary.org/entry/20100406/p1
//- https://ja.wikipedia.org/wiki/%E3%83%8F%E3%83%83%E3%82%B7%E3%83%A5%E3%83%86%E3%83%BC%E3%83%96%E3%83%AB
//- https://github.com/rui314/chibicc

//ハッシュ関数の種類
typedef enum {
    HASH_MAP_FUNC_FNV_1A,   //FNV-1a hash (Default)
    HASH_MAP_FUNC_FNV_1,    //FNV-1 hash
    HASH_MAP_FUNC_DBG,      //DEBUG
} HASH_MAP_FUNC_TYPE_t;

//ハッシュ関数の種類を設定するグローバル変数
extern HASH_MAP_FUNC_TYPE_t hash_map_func;

//ハッシュエントリ
typedef struct hash_entry HASH_ENTRY_t;
typedef struct hash_entry {
    HASH_ENTRY_t *next;
    void *data;             //ハッシュデータ（任意のポインタ）
    char key[];             //ハッシュキー（文字列）
} HASH_ENTRY_t;

//ハッシュマップ本体
typedef struct {
    int num;                //データ数
    int capacity;           //配列の最大数
    HASH_ENTRY_t **array;   //配列
} HASH_MAP_t;

//ハッシュマップを作成する。
HASH_MAP_t *new_hash_map(void);

//ハッシュマップをフリーする。
void free_hash_map(HASH_MAP_t *hash_map);

//データ書き込み
//すでにデータが存在する場合は上書きし0を返す。新規データ時は1を返す。
//キーにNULLは指定できない。dataにNULLを指定できる。
int put_hash_map(HASH_MAP_t *hash_map, const char *key, void *data);

//キーに対応するデータの取得
//存在すればdataに値を設定して1を返す。
//存在しなければ0を返す。
int get_hash_map(HASH_MAP_t *hash_map, const char *key, void **data);

//データの削除
//キーに対応するデータを削除して1を返す。
//データが存在しない場合は0を返す。
int del_hash_map(HASH_MAP_t *hash_map, const char *key);

//ハッシュマップをダンプする
//level=0: 基本情報のみ
//level=1: 有効なキーすべて
void dump_hash_map(const char *str, HASH_MAP_t *hash_map, int level);
