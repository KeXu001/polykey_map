# polykey_map
### Many-to-one C++ container

`xu::polykey_map<Value_T, Key_Ts...>` implements a container analogous to a table in a relational database where:

| Key 1 | Key 2 | Value |
| ----- | ----- | ----- |
| 15 |  | "Hello" |
|  | 'F' | ", " |
| 16 | 'D' | "World!" |

- There is one value column and multiple nullable key columns
- Each row must have at least one valid key
- Value retrieval is done by specifying a key column and a valid key
- Removal is at the row level, i.e. removing where key1=16 also results in the removal of key='D'
- Keys are unique within a column

### Behavior

Member functions take a column index as a template parameter and a key as a function parameter.

- `void insert<index>(key)`
- `Value_T& get<index>(key)`
- `bool contains<index>(key)`

The link function takes an additional index and key.

- `void link<index1, index2>(key1, key2)`

New values are inserted with a single key. To add a new key for an existing value, the `link` function is used.

Example code:

```
enum Keys
{
  Key1,
  Key2
};

/* the first template argument is the stored value type */
xu::polykey_map<std::string, int, char> pkmap;

pkmap.insert<Key1>(15, "Hello");
pkmap.insert<Key2>('F', ", ");

pkmap.insert<Key1>(16, "World!");
pkmap.link<Key1, Key2>(16, 'D');

...

pkmap.at<Key2>('D') = "there!"

pkmap.erase<Key1>(16);

if (pkmap.contains<Key1>(17))
{
  ...
}

for (auto& s : pkmap)
{
  ...
}
```
