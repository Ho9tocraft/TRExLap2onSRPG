# ユニットデータ パース情報
## 1個目の項目(無題部分): ルート
```toml
schema_version = "1.0"
type = "units"
id = "Unit ID here"
tags = ["tags here"]
image = "Unit Image here(NEED png)"
bgm = "BGM here(NEED ogg or mp3)"
bgm_priority = 0
```
- schema_version: tomlのスキーマバージョン。1.0固定。
- type: データの種類。ユニットデータは "units" 固定。
- id: ユニットの一意な識別子。文字列。ダブってはいけない、また、空文字列も不可で英数字とアンダースコアのみで構成されることが望ましい。
- tags: ユニットのタグ。文字列の配列。空配列も可。
- image: ユニットの画像ファイル名。png形式であることが望ましい。assets/images内に配置される。  
  絶対パスや`..`の使用はできない。
- bgm: ユニットのBGMファイル名。oggまたはmp3形式であることが望ましい。assets/audio内に配置される。  
  絶対パスや`..`の使用はできない。
- bgm_priority: ユニットのBGMの優先度。整数値。  
  `-5`～`15`の範囲で、`-5`は最も低くフィールドBGMである場合が多く、  
  `15`は最も高くラスボス戦の主題歌とかその辺。  
  なお、このケースで言うと、`Trombe!(レーツェル・ファインシュメッカーのアレ)`は`14`となる。

つまり、`schema_version`は無視可能(パース時のみの参照データ)、  
`type`はunits指定が成されているかを確認すればよいだけ(`schema_version`と同じ)、  
`id`は正規表現かませた後ユニークであればよい、`tags`は空配列でもよいのでオプションで…、  
`image`と`bgm`は`assets/images`と`assets/audio`に存在するかを確認すればよい、`bgm_priority`は-5～15の範囲であればよい、ということになる。
また、`bgm_priority`が範囲逸脱を起こしても、「-5未満ならば-5」「15より大きいならば14」と誤魔化すことが可能。
15より大きいならば15とはできない(主題歌BGMが流れたときに、後勝ちできてしまうため)。

型は以下の通り。
`schema_version`及び`type`: 無視(DataUnitsの実クラスには保存しない)
`id`: std::u8string
`tags`: `std::vector<std::u8string>`
`image`: std::u8string
`bgm`: std::u8string
`bgm_priority`: int8_t (-5～15) ※範囲が狭いので、int8_tで十分。int16_tでも可。  
  範囲逸脱において警告メッセージ等は発生しない(ゲームエンジンではないため)。

## 2個目の項目(ディスプレイ): main項
```toml
name_ja = "Unit Name here"
name_en = "Unit Name here"
desc_ja = ["Unit Description here"]
desc_en = ["Unit Description here"]
```
- name_ja: ユニットの日本語名。文字列。空文字列は不可。
- name_en: ユニットの英語名。文字列。空文字列は不可。他言語のフォールバック。
- desc_ja: ユニットの日本語説明文。文字列の配列。空配列は非推奨。
- desc_en: ユニットの英語説明文。文字列の配列。空配列は非推奨。他言語のフォールバック。

C++では、すべてstd::u8string(descは`std::vector<std::u8string>`)で保持することになる。

## 3個目の項目(基礎ステータス): baseStats項
※Unit→HP, Pilot→MELのように読み込む
```toml
Unit.HP = 4000 # 最大HP
Unit.MP = 100 # 最大MP
Unit.DEF = 1000 # 装甲値
Unit.MOB = 100 # 運動性
Unit.SIG = 100 # 照準値
Unit.MVR = 6 # 移動力
Unit.MVT = [0, 1] # 移動タイプ(-1:地中, 0:地上, 1:飛行, 2:水中, 3:水上, 4:宇宙/異空間)
Unit.ADP = [5, 5, 4, 5] # 地形適応(空, 陸, 海, 宇の順)
Pilot.PSL = -1 # 性格 (-1:機械, 0:普通, 1:強気, 2:超強気, 3:冷静, 4:慎重, 5:楽天家, 6:努力家, 7:短気, 8:大物, 9:超大物, 10:狡猾, 11:残虐, 12:残火, 13:狂気)
Pilot.MEL = 145 # 格闘
Pilot.RNG = 145 # 射撃
Pilot.MAG = 145 # 魔力
Pilot.DEX = 160 # 技量
Pilot.DEF = 110 # 防御
Pilot.AVD = 158 # 回避
Pilot.RST = 150 # 抵抗
Pilot.ACC = 160 # 命中
Pilot.SPP = 40 # 精神ポイント
Pilot.SyncedUnit = "Unit ID here" # 同期ユニットID
```
- Unit.HP: ユニットの最大HP。整数値。0以下は不可。
- Unit.MP: ユニットの最大MP。整数値。0以下は不可。
- Unit.DEF: ユニットの装甲値。整数値。0以下は不可。
- Unit.MOB: ユニットの運動性。整数値。0以下は不可。
- Unit.SIG: ユニットの照準値。整数値。0以下は不可。
- Unit.MVR: ユニットの移動力。整数値。0以下は不可。
- Unit.MVT: ユニットの移動タイプ。整数値の配列or文字列。空配列は非推奨。  
  -1: 地中, 0: 地上, 1: 飛行, 2: 水中, 3: 水上, 4: 宇宙/異空間  
  また、「空陸水」とすることも可能(この場合は、-1と3が指定できない)。
- Unit.ADP: ユニットの地形適応。整数値の配列。空配列は非推奨。  
  配列の順番は、空, 陸, 海, 宇の順であること。  
  0: F(使用不可), 1: E(20％), 2: D(40％), 3: C(60％), 4: B(80％), 5: A(100％), 6: S(120％)
- Pilot.PSL: パイロットの性格。整数値または文字列。  
  整数値である場合は、-1～13の範囲で、-1は機械、0は普通、1は強気、2は超強気、3は冷静、4は慎重、5は楽天家、  
  6は努力家、7は短気、8は大物、9は超大物、10は狡猾、11は残虐、12は残火、13は狂気。
- Pilot.GND: パイロットの性別。整数値または文字列。空文字列は不可。  
  整数値である場合は、-2～1の範囲で、-2は機械、-1は不明、0は男性、1は女性。  
  文字列である場合は、"Male"は男性、"Female"は女性、"Machine"は機械。それ以外は一律で「不明」。
- Pilot.MEL: パイロットの格闘値。整数値。0以下は不可。
- Pilot.RNG: パイロットの射撃値。整数値。0以下は不可。
- Pilot.MAG: パイロットの魔力値。整数値。0以下は不可。
- Pilot.DEX: パイロットの技量値。整数値。0以下は不可。
- Pilot.DEF: パイロットの防御値。整数値。0以下は不可。
- Pilot.AVD: パイロットの回避値。整数値。0以下は不可。
- Pilot.RST: パイロットの抵抗値。整数値。0以下は不可。
- Pilot.ACC: パイロットの命中値。整数値。0以下は不可。
- Pilot.SPP: パイロットの精神ポイント。整数値。0未満は不可。  
  0は、精神コマンドが設定されていても「精神なし」と見做される。
- Pilot.SyncedUnit: セーブデータ上でのパイロットステータスの同期ユニットID。  
  文字列の配列。もしくは文字列（1体しか同期ユニットが存在しない場合）。同期ユニットが存在する場合の空配列は不可。  
  同期ユニットが存在しない場合は、空配列を指定することも可能。

型指定は以下の通り。
`unitHP`, `unitMP`, `unitDEF`, `unitMOB`, `unitSIG`, `unitMVR`: int64_t
`unitMVT`: `std::vector<EnumStageMovableType>` ※Enum Class列挙型に変形させる
`unitADP`: `std::vector<EnumTerrainAdapt>` ※Enum Class列挙型に変形させる
`pilotPSL`: EnumPilotPersonalityType ※Enum Class列挙型に変形させる
`pilotGND`: EnumPilotGenderType ※Enum Class列挙型に変形させる
`pilotMEL`, `pilotRNG`, `pilotMAG`, `pilotDEX`, `pilotDEF`, `pilotAVD`, `pilotRST`, `pilotACC`, `pilotSPP`: int64_t
`syncedUnit`: `std::vector<std::u8string>` ※単一時は最終的に1要素の配列として扱う

## 4個目の項目(ロードアウト): loadout項
```toml
job_id = "Job ID here" # ジョブID
eikonic_slot_count = 0 # 召喚獣スロット数
weapon_cost_limit = 200 # 武器コスト上限
carry_item_slots = 2 # 持ち込みアイテムスロット数
modify_type = "Modify Type here" # 改造タイプ(COMMON, MELEE, RANGE, CASTER)
```
- job_id: ユニットのデフォルトジョブID。文字列。空文字列は不可。
- eikonic_slot_count: ユニットの召喚獣スロット数。整数値。  
  0以上であること。基本的に、PCユニットは0、召喚獣ユニットは1以上であることが望ましい。
- weapon_cost_limit: ユニットの武器コスト上限。整数値で0以上。
- carry_item_slots: ユニットの持ち込みアイテムスロット数。整数値で0以上。最大でも4に留めた方がいいだろう。
- modify_type: ユニットの改造タイプ。文字列。空文字列は不可。  
  COMMON, MELEE, RANGE, CASTERのいずれかであること。

型指定は以下の通り。
`job_id`: EnumTRExLap2Job ※Enum Class列挙型に変形させる
`eikonic_slot_count`, `weapon_cost_limit`, `carry_item_slots`: uint64_t
`modify_type`: EnumModifyType ※Enum Class列挙型に変形させる

## 5個目の項目(特殊効果配列): effects項
```toml
effect_id = "Effect ID here" # 特殊能力ID
effect_type = "Effect Type here" # 特殊能力タイプ
effect_display_name_ja = "Effect Display Name here" # 特殊能力表示名(日本語, 空欄でデフォルト表示)
effect_display_name_en = "Effect Display Name here" # 特殊能力表示名(英語, 空欄でデフォルト表示)
```
- effect_id: ユニットの特殊能力ID。文字列。空文字列は不可。非重複。
- effect_type: ユニットの特殊能力タイプ。文字列。空文字列は不可。  
- effect_display_name_ja: ユニットの特殊能力表示名(日本語)。文字列。
- effect_display_name_en: ユニットの特殊能力表示名(英語)。文字列。

型指定は以下の通り。
`effect_id`, `effect_display_name_ja`, `effect_display_name_en`: std::u8string
`effect_type`: EnumTRExLap2EffectType ※Enum Class列挙型に変形させる

## 6個目の項目(特殊スキル配列): skills項
```toml
skill_id = "Skill ID here" # スキルID
skill_type = "Skill Type here" # スキルタイプ
skill_display_name_ja = "Skill Display Name here" # スキル表示名(日本語, 空欄でデフォルト表示)
skill_display_name_en = "Skill Display Name here" # スキル表示名(英語, 空欄でデフォルト表示)
skill_hasLevel = true # スキルレベルの有無
skill_level = [1] # スキルレベル
skill_learn_level = [1] # スキル習得レベル
```
- skill_id: ユニットのスキルID。文字列。空文字列は不可。非重複。
- skill_type: ユニットのスキルタイプ。文字列。空文字列は不可。
- skill_display_name_ja: ユニットのスキル表示名(日本語)。文字列。
- skill_display_name_en: ユニットのスキル表示名(英語)。文字列。
- skill_hasLevel: ユニットのスキルレベルの有無。真偽値。  
  trueならば、`skill_level`が配列として有効となる(無効時は、skill_learn_levelの最初の要素だけが有効となる)。
- skill_level: ユニットのスキルレベル。整数値の配列。空配列は非推奨。  
  `skill_hasLevel`がtrueである場合のみ有効。
- skill_learn_level: ユニットのスキル習得レベル。整数値の配列。空配列は非推奨。  
  `skill_hasLevel`がtrueである場合のみ、2個目以降の要素が有効。

型指定は以下の通り。
`skill_id`, `skill_display_name_ja`, `skill_display_name_en`: std::u8string
`skill_type`: EnumTRExLap2SkillType ※Enum Class列挙型に変形させる
`skill_hasLevel`: bool
`skill_level`, `skill_learn_level`: std::vector<int64_t>

## 7個目の項目(精神コマンド): spiritual_command項
```toml
sp_id = "Spiritual Command ID here" # 精神コマンドID
sp_learn_level = 1 # 精神コマンド習得レベル (1～99 or 0(ツイン精神))
sp_cost = 0 # 精神コマンド消費値(0はデフォルト値)
```
- sp_id: ユニットの精神コマンドID。文字列。空文字列は不可。使用可能なものは以下の通り。  
  わかりやすさの観点から、日本語指定が望ましいだろう(そのままキーになる)。
```
魂, 闘志, 熱血,
鉄壁, 不屈, 信念, 強靱,
感応, 先見, 集中, 必中, 閃き,
覚醒, 再動, 加速, 疾風, 連撃,
祈り, 絆, 期待, 根性, 信頼, ド根性, 友情,
気合, 気迫, 激励, 大激励,
重撃, 狙撃, 直撃, 手加減, 同調, 突撃,
威圧, 撹乱, 偵察, 分析,
応援, 幸運, 修行, 祝福, 努力,
愛, 強襲, 奇跡, 希望, 切り札, 直感, 勇気,
決意, 想念
```
- sp_learn_level: ユニットの精神コマンド習得レベル。整数値。1～99の範囲であること。  
  0はツイン精神コマンドを意味する。
- sp_cost: ユニットの精神コマンド消費値。整数値。0以上であること。  
  このとき、0はデフォルト値を意味する。

型指定は以下の通り。
`sp_id`: EnumTRExLap2SPType ※Enum Class列挙型に変形させる
`sp_learn_level`, `sp_cost`: int64_t

## 8個目の項目(リレーション補正): relationships項
※WIP
```toml
relationship_id = "Relationship ID here" # リレーション補正ID
relationship_type = "Relationship Type here" # リレーション補正タイプ
relationship_target = "Relationship Target here" # リレーション補正対象
relationship_level = 1 # リレーション補正レベル
```
- relationship_id: ユニットのリレーション補正ID。文字列。空文字列は不可。非重複。
- relationship_type: ユニットのリレーション補正タイプ。文字列。空文字列は不可。
- relationship_target: ユニットのリレーション補正の対象となるユニットID。文字列。空文字列は不可。
- relationship_level: ユニットのリレーション補正レベル。整数値。1以上であること。

型指定は以下の通り。
`relationship_id`, `relationship_target`: std::u8string
`relationship_type`: EnumTRExLap2RelationshipType ※Enum Class列挙型に変形させる
`relationship_level`: int64_t

## 9個目の項目(武装配列): weapons項
```toml
weapon_id = "Weapon ID here" # 武器ID
weapon_name_ja = "Weapon Name here" # 武器名(日本語)
weapon_name_en = "Weapon Name here" # 武器名(英語)
weapon_type = "Weapon Type here" # 武器タイプ(MEL:格闘、RNG:射撃, MAG:魔法)
weapon_category = ["Weapon Category here"] # 武器カテゴリ(F:支援武器, C:コンビネーション武器, ALL:ALL武器, ALL/W:ダブルアタック, MAP:MAP兵器（オプションあり）。_は値なし(カテゴリなし))
# 武器属性
# P:移動後使用可能, SpTk:気力ダウン(L1～3), SpDr:SP吸収(L1～3), WpBr:攻撃ダウン(L1～3),
# MPTk:MPダウン(L1～3), MPDr:MP吸収(L1～3), DFBr: 装甲ダウン(L1～3), MoBr:運動性ダウン(L1～3),
# ChGr:照準値ダウン(L1～3), SpNt:移動力ダウン(L1～3), BaBr:バリア貫通, DisB: バリア無効, _:なし
weapon_attribute = ["Weapon Attribute here"]
weapon_attack = 2000 # 武器攻撃力
min_range = 1 # 最小射程
max_range = 1 # 最大射程
consume_type = "Consume Type here" # 消費タイプ(NONE, HP, MP, HPMP, BRT)
max_bullet = 1 # 最大弾数 (BRT武器のみ)
consume_value = 0 # 消費値 (HP, MP, HPMP武器のみ。HPMP武器ではHPコスト)
consume_value2 = 0 # 消費値2 (HPMP武器のみ。HPMP武器ではMPコスト)
recast_timer = 0 # リキャストタイマー
recast_timer_type = "Recast Timer Type here" # リキャストタイマータイプ(NONE, SOLO, GCD, SYNC_X。Xは10進数数値)
recast_charge_act = false # チャージアクションかどうか
recast_charge_act_max = 0 # チャージアクションの最大チャージ数
need_morale = 0 # 必要気力。100以下で必要気力なしと同義
accurary_adapt = 0 # 命中補正値
critical_adapt = 0 # クリティカル補正値
weapon_tadapt = [5,5,5,5] # 武器地形適応(0:F(使用不可), 1:E(20％), 2:D(40％), 3:C(60％), 4:B(80％), 5:A(100％), 6:S(120％) / 空,陸,海,宇)
```
- weapon_id: ユニットの武器ID。文字列。空文字列は不可。非重複。
- weapon_name_ja: ユニットの武器名(日本語)。文字列。空文字列は不可。
- weapon_name_en: ユニットの武器名(英語)。文字列。空文字列は不可。
- weapon_type: ユニットの武器タイプ。文字列。空文字列は不可。MEL(格闘)、RNG(射撃)、MAG(魔法)のいずれか。
- weapon_category: ユニットの武器カテゴリ。文字列の配列。空配列は非推奨。  
  F: 支援武器, C: コンビネーション武器, ALL: ALL武器, ALL/W: ダブルアタック, MAP: MAP兵器（オプションあり）。  
  _は値なし(カテゴリなし)。
- weapon_attribute: ユニットの武器属性。文字列の配列。空配列は非推奨。  
  P: 移動後使用可能, SpTk: 気力ダウン(L1～3), SpDr: SP吸収(L1～3), WpBr: 攻撃ダウン(L1～3),  
  MPTk: MPダウン(L1～3), MPDr: MP吸収(L1～3), DFBr: 装甲ダウン(L1～3), MoBr: 運動性ダウン(L1～3),  
  ChGr: 照準値ダウン(L1～3), SpNt: 移動力ダウン(L1～3), BaBr: バリア貫通, DisB: バリア無効, _: なし
- weapon_attack: ユニットの武器攻撃力。整数値。0以上であること。1000以上が望ましい。
- min_range: ユニットの武器最小射程。整数値。1以上であること。0は実装予定のアビリティにのみ許諾される。
- max_range: ユニットの武器最大射程。整数値。1以上であること。  
  min_range以上であること。max_rangeがmin_rangeより小さい場合は、min_rangeと同値に置き換えられる。
- consume_type: ユニットの武器消費タイプ。文字列。空文字列は不可。
  NONE(消費なし), HP(HPコスト), MP(MPコスト), HPMP(HPMP双方コスト), BRT(弾数制)のいずれか。
- max_bullet: ユニットの武器最大弾数。整数値。0以上であること。  
  BRT武器のみ有効。BRT武器以外では、0に置き換えられる。
- consume_value: ユニットの武器消費値。整数値。0以上であること。HP、MP、HPMP武器のみ有効。HPMP武器以外では、0に置き換えられる。
- consume_value2: ユニットの武器消費値2。整数値。0以上であること。  
  HPMP武器のみ有効。HPMP武器以外では、0に置き換えられる。
- recast_timer: ユニットの武器リキャストタイマー。整数値。0以上であること。
- recast_timer_type: ユニットの武器リキャストタイマータイプ。文字列。空文字列は不可。  
  NONE(リキャストなし), SOLO(単独リキャスト), GCD(グローバルリキャスト), SYNC_X(同期リキャスト、Xは10進数数値)のいずれか。
- recast_charge_act: ユニットの武器チャージアクションかどうか。真偽値。
- recast_charge_act_max: ユニットの武器チャージアクションの最大チャージ数。整数値。  
  2以上であること(1は通常のリキャストタイマーであるため、不可)。
- need_morale: ユニットの武器必要気力。整数値。0以上であること。
- accurary_adapt: ユニットの武器命中補正値。整数値。0以上であること。
- critical_adapt: ユニットの武器クリティカル補正値。整数値。0以上であること。
- weapon_tadapt: ユニットの武器地形適応。整数値の配列。空配列は非推奨。  
  0: F(使用不可), 1: E(20％), 2: D(40％), 3: C(60％), 4: B(80％), 5: A(100％), 6: S(120％)  
  配列の順番は、空, 陸, 海, 宇の順であること。

型指定は以下の通り。
`weapon_id`, `weapon_name_ja`, `weapon_name_en`: std::u8string
`weapon_type`: EnumTRExLap2WeaponType ※Enum Class列挙型に変形させる
`weapon_category`: `std::vector<EnumTRExLap2WeaponCategory>` ※Enum Class列挙型に変形させる
`weapon_attribute`: `std::vector<EnumTRExLap2WeaponAttribute>` ※Enum Class列挙型に変形させる
`weapon_attack`, `min_range`, `max_range`, `max_bullet`, `consume_value`, `consume_value2`, `recast_timer`, `recast_charge_act_max`, `need_morale`, `accurary_adapt`, `critical_adapt`: int64_t
`consume_type`: EnumTRExLap2WeaponConsumeType ※Enum Class列挙型に変形させる
`recast_timer_type`: EnumTRExLap2WeaponRecastTimerType ※Enum Class列挙型に変形させる
`weapon_tadapt`: `std::vector<EnumTerrainAdapt>` ※Enum Class列挙型に変形させる

## 10個目の項目(アビリティ配列): abilities項
Optional。そもそもない可能性もある
```toml
ability_id = "Ability ID here" # アビリティID
ability_name_ja = "Ability Name here" # アビリティ名(日本語)
ability_name_en = "Ability Name here" # アビリティ名(英語)
ability_effect = ["Ability Effect here"] # アビリティ効果
ability_category = ["Ability Category here"] # アビリティカテゴリ(武器と同じ)
ability_attribute = ["Ability Attribute here"] # アビリティ属性(武器と同じ)
min_range = 1 # 最小射程
max_range = 1 # 最大射程
consume_type = "Consume Type here" # 消費タイプ(NONE, HP, MP, HPMP, BRT)
max_bullet = 1 # 最大弾数 (BRT武器のみ)
consume_value = 0 # 消費値 (HP, MP, HPMP武器のみ。HPMP武器ではHPコスト)
consume_value2 = 0 # 消費値2 (HPMP武器のみ。HPMP武器ではMPコスト)
recast_timer = 0 # リキャストタイマー
recast_timer_type = "Recast Timer Type here" # リキャストタイマータイプ(NONE, SOLO, GCD, SYNC_X。Xは10進数数値)
recast_charge_act = false # チャージアクションかどうか
recast_charge_act_max = 0 # チャージアクションの最大チャージ数
need_morale = 0 # 必要気力。100以下で必要気力なしと同義
```
- ability_id: ユニットのアビリティID。文字列。空文字列は不可。非重複。
- ability_name_ja: ユニットのアビリティ名(日本語)。文字列。空文字列は不可。
- ability_name_en: ユニットのアビリティ名(英語)。文字列。空文字列は不可。
- ability_effect: ユニットのアビリティ効果。文字列の配列。空配列は不可。
- ability_category: ユニットのアビリティカテゴリ。文字列の配列。空配列は非推奨。  
  ALL: ALL武器, ALL/W: ダブルアタック, MAP: MAP兵器（オプションあり）。_は値なし(カテゴリなし)。  
  これ以外は弾かれる(それしかない場合は、カテゴリなしと見做される)。
- ability_attribute: ユニットのアビリティ属性。文字列の配列。空配列は非推奨。  
  P: 移動後使用可能, SpTk: 気力ダウン(L1～3), SpDr: SP吸収(L1～3), WpBr: 攻撃ダウン(L1～3),  
  MPTk: MPダウン(L1～3), MPDr: MP吸収(L1～3), DFBr: 装甲ダウン(L1～3), MoBr: 運動性ダウン(L1～3),  
  ChGr: 照準値ダウン(L1～3), SpNt: 移動力ダウン(L1～3), BaBr: バリア貫通, DisB: バリア無効, _: なし
- min_range: ユニットのアビリティ最小射程。整数値。0以上であること。
- max_range: ユニットのアビリティ最大射程。整数値。1以上であること。  
  min_range以上であること。max_rangeがmin_rangeより小さい場合は、min_rangeと同値に置き換えられる。
- consume_type: ユニットのアビリティ消費タイプ。文字列。空文字列は不可。
- max_bullet: ユニットのアビリティ最大弾数。整数値。0以上であること。  
  BRT武器のみ有効。BRT武器以外では、0に置き換えられる。
- consume_value: ユニットのアビリティ消費値。整数値。0以上であること。HP、MP、HPMP武器のみ有効。HPMP武器以外では、0に置き換えられる。
- consume_value2: ユニットのアビリティ消費値2。整数値。0以上であること。  
  HPMP武器のみ有効。HPMP武器以外では、0に置き換えられる。
- recast_timer: ユニットのアビリティリキャストタイマー。整数値。0以上であること。
- recast_timer_type: ユニットのアビリティリキャストタイマータイプ。文字列。空文字列は不可。  
  NONE(リキャストなし), SOLO(単独リキャスト), GCD(グローバルリキャスト), SYNC_X(同期リキャスト、Xは10進数数値)のいずれか。
- recast_charge_act: ユニットのアビリティチャージアクションかどうか。真偽値。
- recast_charge_act_max: ユニットのアビリティチャージアクションの最大チャージ数。整数値。  
  2以上であること(1は通常のリキャストタイマーであるため、不可)。
- need_morale: ユニットのアビリティ必要気力。整数値。0以上であること。

型指定は現状未定。

## 11個目の項目(エースボーナス): aceBonus項
```toml
ace_bonus_id = "ace_bonus_exellia" # エースボーナスID
```
- ace_bonus_id: ユニットのエースボーナスID。文字列。空文字列は不可。非重複。

型指定は以下の通り。
`ace_bonus_id`: std::u8string
