# ユニットTOMLの確認とパース用ライブラリ方針

作成日: 2026-08-22  
状態: draft

## 結論

ゲーム実行時のユニット定義パースには **toml++ 3.x** を必須導入する。
追加依存は最小限にとどめ、診断品質とデータ回帰試験のために **fmt 11.x 以上** と **Catch2 3.x** を推奨する。

```text
必須:     toml++
推奨:     fmt
推奨:     Catch2
導入不要: YAMLパーサ、JSONパーサ、utf8cpp、Pythonランタイム
```

TOMLを正規データ形式としているため、JSON/YAMLパーサをゲーム本体に重ねて導入しない。Python 3は将来、静的検証やバランス試算を行う開発ツールに限定する。

## 各ライブラリの役割

| ライブラリ | 区分 | 用途 | 採用理由 |
|---|---|---|---|
| toml++ 3.x | 必須 | UTF-8 TOMLのパース、構文エラー位置の取得、TOMLツリーの走査 | C++17以降対応、ヘッダオンリー運用可、TOML 1.0.0対応。現行C++20・MSVCプロジェクトに合う。 |
| fmt 11.x以上 | 推奨 | データ検証エラー、ロードログ、開発者向け診断文の整形 | `path:line:column` とID・期待値を組み合わせた、読めるエラー表示を作る。ゲームデータの不備を起動時に特定しやすくする。 |
| Catch2 3.x | 推奨 | ユニット定義のロード試験、異常データの拒否試験、参照整合性試験 | データ形式の変更時に、全TOMLが正しく読めることを自動確認する。リリース実行ファイルにはリンクしない。 |

### 導入しないもの

- **YAMLパーサ**: 正規形式をTOMLへ決めたため不要。型解釈とインデント由来の曖昧さを増やさない。
- **JSONパーサ**: エディタ連携・外部ツール出力が必要になるまで追加しない。TOMLの読み込みだけに使うなら不要。
- **utf8cpp**: toml++はUTF-8を扱えるため、TOMLファイルの読み込みだけなら重複する。将来、UIの文字単位折返しや書記素単位の操作が必要になった時点で再検討する。
- **Pythonランタイム**: 本体へ埋め込まない。Python 3.10が環境にあってもTOML標準モジュール `tomllib` はPython 3.11からであり、現時点のローカルPythonでの簡易検証には使えない。

## 導入方法

依存管理はvcpkgのmanifest modeを推奨する。導入実施時に、リポジトリ直下へ `vcpkg.json` を追加する。

```json
{
  "name": "trexlap2-on-srpg",
  "version-string": "0.1.0",
  "dependencies": [
    "tomlplusplus",
    "fmt",
    "catch2"
  ]
}
```

ただし、この文書作成時点では依存ファイルおよびVisual Studioプロジェクト設定には変更を加えない。

## ローダの責務

toml++は構文を読むだけに使い、ゲーム固有の妥当性はC++の `UnitDataLoader` が検査する。

```text
TOMLファイル
  -> toml++: UTF-8・構文・テーブル構造を確認
  -> UnitDataLoader: 必須キー、整数範囲、列挙値、配列長を確認
  -> DataRegistry: ID重複・参照先・他データとの整合性を確認
  -> 読み取り専用のUnitDefinitionとしてゲームへ渡す
```

エラーは可能な限りファイル名・行・列・ID・期待値を表示する。

```text
data/units/Exellia-normal.toml:143: weapon_tadapt must contain 4 values; got 3
```

## 既存Exellia定義の確認

確認対象:

- `TRExLap2-Project/data/units/Exellia-normal.toml`
- `TRExLap2-Project/data/units/Exellia-semiprime.toml`

両ファイルは、以下の構造を共通して持つ。

- `main`: 日英の名称・説明
- `baseStats`: ユニット能力とパイロット能力
- `loadout`: ジョブ、召喚獣スロット、武器コスト、持ち込み枠、改造型
- `effects` / `skills` / `relationships`: 固有能力、習得スキル、関係性
- `weapons`: 通常戦闘武器、MAP兵器、共通武装、形態固有武装
- `aceBonus`: エースボーナス参照

通常形態と半顕現形態は、共通武装を持ちつつ、後者が専用武装・武器火力・一部の射程／地形適応で上位となる、明瞭な形態差になっている。これは「通常形態で盤面を回し、半顕現で決着力を出す」というコンセプトと整合する。

### 実装前にローダで必ず検証する項目

1. `baseStats` の `Unit.HP` などはTOMLのドット付きキーである。ローダは `baseStats.Unit.HP` のようなネスト構造として取得する。
2. `skill_level` と `skill_learn_level` は必ず同じ要素数にする。
3. `min_range <= max_range` を要求する。
4. `weapon_tadapt` は空・陸・海・宇宙の4要素固定とする。
5. `mapw_type`、`mapw_ipoint_shape`、`mapw_iff` はMAP武器以外では禁止し、MAP武器では型別に必須項目を定める。
6. `consume_type` に応じて、弾数・HP・MPの各コスト項目を検証する。
7. `recast_charge_act = false` のとき、`recast_charge_act_max` は0とする。
8. `id`（ユニットID）は全データベースで一意とする。一方、`weapon_id`・`skill_id`・`effect_id`は全ユニットで再利用できるため、ユニット定義内でのみ重複を禁止する。
9. 武器を実行時に一意に識別するキーは `unit_id + weapon_id` とする。`exellia_common_*` のような同名武器は、形態ごとに個別の性能を持てる。
10. `job_id`、`effect_type`、`ace_bonus_id`など、外部データを参照する値は参照先が存在することを確認する。

## 最初のテスト方針

Catch2で以下を自動テストする。

1. `Exellia-normal.toml` を正常にロードできる。
2. `Exellia-semiprime.toml` を正常にロードできる。
3. 全ユニットTOMLを走査して、ID重複と未解決参照がない。
4. 故意に壊したfixtureを読み込ませ、エラー位置と原因が返る。
5. 将来的にロード済みデータをバイナリキャッシュ化した場合、TOML読み込み結果との同値性を確認する。

## 参照

- toml++: <https://marzer.github.io/tomlplusplus/>
- fmt: <https://github.com/fmtlib/fmt/blob/main/doc/api.md>
- Catch2: <https://github.com/catchorg/Catch2>
