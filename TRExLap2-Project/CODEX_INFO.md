# Codexへの指示情報
## リソースファイル
以下のように分類されたし。
```plaintext
リソース ファイル
├シェーダー: fragやvertなどのシェーダー。
├ドラフト: in-draft内ファイル群。.gitignore済み。
├草案データ: shared-draft内ファイル群。
├音楽ファイル: assets/audio内のMP3やOGGなどの音楽ファイル。
│　assets/sound内のファイルも含む。**Codexは触らない**。.gitignore済み。
├画像ファイル: assets/images内のPNGなどの画像ファイル。**Codexは基本的に触らない**。
│　指示があった場合は、Codexが触ることもある。
└データ: data内のtomlなどのデータファイル。雛形はビルドデータに含めない。
  ├ユニットデータ: data/units内のユニットデータtoml。`Units.toml`は雛形。
  │　スーパー生身大戦の体なので、パイロットデータとイコールとなる。
  ├ジョブデータ: data/jobs内のジョブデータtoml。`Jobs.toml`は雛形。
  │　ロールアクションもここ。
  ├召喚獣データ: data/eikons内の召喚獣(アビリティ)データtoml。`Eikons.toml`は雛形。
  ├武器データ: data/weapons内の武器データtoml。`Weapons.toml`は雛形。
  ├武器戦技データ: data/aows内の武器戦技データtoml。`AoWs.toml`は雛形。
  ├マテリアデータ: data/materias内のマテリアデータtoml。`Materias.toml`は雛形。
  ├マップデータ: data/maps内のマップデータtoml。`Maps.toml`は雛形。
  │　遠景MAP方式のため、マップデータでのマップ画像は1枚絵となっているはずである。
  ├アイテムデータ: data/items内の持込アイテム(強化パーツ)データtoml。
  │　`Items.toml`は雛形。
  └シナリオデータ: data/scenarios内のシナリオデータLua。`Scenarios.lua`は雛形。
```


