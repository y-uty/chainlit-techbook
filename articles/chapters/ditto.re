= CustomElementを使って絵しりとりを作る！

//lead{
Chainlitのチャット画面の中にお絵かきキャンバスを埋め込み、AIと絵しりとりで遊べるWebアプリを作ります。
標準のMessage/Actionだけではできない、CustomElementという仕組みを主役にした実装を紹介します。標準的なチャットUIを超えた、リッチな機能を実装したい人におすすめの章です。
//}

//pagebreak

== この章で作るもの

この章では、ユーザーがキャンバスに絵を描き、AIが「しりとり」のルールで次の絵を返す、いわゆる「絵しりとり」Webアプリを、Chainlitを使って作ります。

//image[ditto-cat][完成イメージ：チャット内にキャンバスが表示され、好きな絵を描いて送信できる]{
//}

絵の描画には、ブラウザのCanvas APIを使用し、AIの応答にはGemini APIを利用します。
このアプリを通じて、ChainlitのCustomElement機能の活用法をお見せしたいと思います。

本章のソースコード全体はサポートページ@<fn>{support}で公開しています。本文中のコードは要点を抜粋したものですので、詳細はそちらを参照してください。
//footnote[support][https://github.com/statditto/chainlit-techbook-support/ch08-e-shiritori]

=== ファイル構成

ファイル構成を見てみましょう。非常にシンプルです。

//table[files][ファイル構成]{
ファイル	役割
-----------------
app.py	Chainlit UIレイヤー
gemini.py	Gemini APIレイヤー
public/elements/DrawCanvas.jsx	お絵かきキャンバス
//}

設定ファイル（@<code>{.chainlit/config.toml}、@<code>{.env}）にも少し手を加えますが、実装の中心はこの3ファイルです。
たったこれだけで、絵しりとりアプリが完成します。本章では、Chainlitでちょっと手の込んだUIを作成するための機能、CustomElementに焦点を当て、Chainlitの基本機能（@<code>{on_chat_start}、@<code>{cl.Message}など）の説明は他章に譲ります。


== CustomElement

=== なぜCustomElementが必要か

Chainlitには@<code>{cl.Message}でテキストや画像を送る機能、@<code>{cl.Action}でボタンを配置する機能があります。
しかし「キャンバスに自由にお絵かき」のようなリッチなインタラクションは、標準の仕組みだけでは実現できません。
そこで登場するのがCustomElementです。
JSX@<fn>{JSX}コンポーネントを書いて@<code>{public/elements/}に置くだけで、チャットのメッセージ内に独自のUIを埋め込めます。

//footnote[JSX][JSXはJavaScript XMLの略で、JavaScript内でHTMLのような構文を使える拡張構文です。Reactなどのフレームワークで広く使われています。]

=== 3つのポイント

CustomElementで押さえるべきポイントは3つだけです。

==== ① JSXファイルを置くだけで認識される

@<code>{public/elements/DrawCanvas.jsx}というファイルを作ると、バックエンドから@<code>{cl.CustomElement(name="DrawCanvas")}で呼び出せるようになります。
ファイル名とname引数が対応する、というシンプルな規約です。

//emlist[バックエンド側（app.py）]{
canvas = cl.CustomElement(
    name="DrawCanvas",  # public/elements/DrawCanvas.jsx に対応
    props={"hint": hint, "round": round_num, "maxRounds": MAX_ROUNDS},
)
await cl.Message(content="描いてね！", elements=[canvas]).send()
//}

==== ② propsでバックエンドからデータを受け取る

バックエンドで指定した@<code>{props}は、JSX内でグローバル変数@<code>{props}として参照できます。
関数の引数ではなくグローバルに注入される点が通常のReactコンポーネントとの違いです。

//emlist[フロントエンド側（DrawCanvas.jsx）][javascript]{
const hint = props.hint || "絵を描いてね！";
const round = props.round || 1;
const maxRounds = props.maxRounds || 5;
//}

==== ③ callActionでフロントエンドからバックエンドに送信する

ユーザーの操作結果をバックエンドに送るには、グローバル関数@<code>{callAction}を使います。

//emlist[フロントエンド側（DrawCanvas.jsx）]{
callAction({
  name: "submit_drawing",
  payload: { image: canvas.toDataURL("image/png") }
});
//}

//emlist[バックエンド側（app.py）]{
@cl.action_callback("submit_drawing")
async def on_submit(action: cl.Action):
    img_data = action.payload.get("image", "")
    # 画像データを処理して次の絵を生成
//}

@<code>{callAction}の@<code>{name}と@<code>{@cl.action_callback}のname引数が対応します。

=== 使えるライブラリ

CustomElementのJSX内では、次のライブラリなどをインポートして利用できます。

 * React（@<code>{useState}、@<code>{useEffect}、@<code>{useRef}など）
 * shadcn/ui（@<code>{@/components/ui/button}など）
 * lucide-react（アイコン）

逆にいえば、任意の外部パッケージを利用できるわけではありません。
ただし、Canvas APIなどのブラウザ標準APIは制限なく利用できるため、描画機能などはこれらを用いて実装可能です。

== お絵かきキャンバスを実装する

いよいよ本章の主役、@<code>{DrawCanvas.jsx}の実装に入ります。
コード全体はサポートページを参照してください。ここでは実装のポイントを3つに絞って解説します。

=== ポイント① Canvas APIでの描画

お絵かきの核となるのは、HTML5 Canvas APIの@<code>{beginPath}→@<code>{moveTo}→@<code>{lineTo}→@<code>{stroke}のサイクルです。

//emlist[描画の基本サイクル]{
const startDraw = (e) => {
  const ctx = canvas.getContext("2d");
  const pos = getPos(e, canvas);
  ctx.beginPath();           // 新しいパスを開始
  ctx.moveTo(pos.x, pos.y);  // 始点を設定
  setIsDrawing(true);
};
const draw = (e) => {
  if (!isDrawing) return;
  const pos = getPos(e, canvas);
  ctx.lineTo(pos.x, pos.y);  // 始点から現在位置まで線を引く
  ctx.stroke();              // 実際に描画
};
//}

@<code>{lineCap = "round"}を設定することで、線の端が丸くなり、手書き風の滑らかな線になります。

=== ポイント② callActionで画像を送信する

「送る」ボタンが押されたら、canvasの内容をバックエンドに送ります。

//emlist[画像の送信]{
const submit = () => {
  if (submitted) return;
  const canvas = canvasRef.current;
  const imageData = canvas.toDataURL("image/png");
  setSubmitted(true);
  callAction({
    name: "submit_drawing",
    payload: { image: imageData }
  });
};
//}

送信後は@<code>{setSubmitted(true)}でボタンとキャンバスを無効化し、二重送信を防止しています。

=== ポイント③ propsで毎ラウンドの情報を受け取る

ラウンドが進むたびに、バックエンドは新しいCustomElementを生成して送ります。
フロントエンドでは@<code>{props}の変化を@<code>{useEffect}で検知して、キャンバスを白紙にリセットしています。

//emlist[propsの変化でキャンバスをリセット]{
useEffect(() => {
  const canvas = canvasRef.current;
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  setSubmitted(false);
}, [props.hint]);  // hintが変わったらリセット
//}

== バックエンドでゲームを動かす

キャンバスから送られてきた画像データを受け取り、Gemini APIで処理し、結果を返すバックエンドの実装を見ていきます。

=== ゲームフローの全体像

ユーザーが「送る」ボタンを押した後のバックエンド処理は、大きく3つのステップで構成されます。

//emlist[app.py：action_callback の要約]{
# 画像データを用意
img_data = action.payload.get("image", "").split(",")[1]
img_bytes = base64.b64decode(img_data)

# 1. AIがユーザーの絵を判定（Step 1）
async with cl.Step(name="判定中...") as step:
    thinking, user_word = await identify_drawing(img_bytes, last_char)
    step.output = thinking

# 2. AIが次の絵を生成（Step 2）
async with cl.Step(name="次を考え中...") as step:
    _, ai_word = await pick_next_word(user_word[-1])
    ai_img_bytes = await generate_image(ai_word)

# 3. 結果表示と次のターンの準備
await cl.Message(content="AIの絵", elements=[...]).send()
await cl.Message(content="続きをどうぞ！", elements=[...]).send()
//}

これは人間が絵しりとりを行うときの思考過程と同じ構造になっています。

=== cl.Stepで「AIの思考」を折りたたみ表示

@<code>{cl.Step}を使うことで、チャット内に折りたたみ可能なセクションを作れます。
//emlist[cl.Stepの基本パターン]{
async with cl.Step(name="🤔 AIが絵を見て考え中...") as step:
    thinking, user_word = await identify_drawing(img_bytes, last_char)
    step.output = thinking  # 折りたたみの中に表示される内容
//}

cl.Stepを活用すると、AIの思考過程を見せながら、チャット画面をすっきり保てます。

//image[ditto-thinking][AIの思考過程]{
//}

=== Gemini APIの構造化出力で応答を安定させる

LLMの出力は自由形式のテキストです。
「思考過程」と「回答の単語」を分離して取得する必要があります。正規表現でパースする方法もありますが、Gemini APIの@<b>{構造化出力}（Structured Output）を使えば確実です。

//emlist[gemini.py：スキーマ定義と利用（抜粋）]{
_SHIRITORI_SCHEMA = {
    "type": "object",
    "properties": {
        "thinking": {"type": "string",
            "description": "5歳の幼稚園児風の短い思考過程（2〜3行）"},
        "answer":   {"type": "string",
            "description": "ひらがなの単語（「ん」で終わらないこと）"},
    },
    "required": ["thinking", "answer"],
}
resp = client.models.generate_content(
    model=VISION_MODEL, contents=[image_part, prompt],
    config=types.GenerateContentConfig(
        response_mime_type="application/json",
        response_schema=_SHIRITORI_SCHEMA,
    ),
)
data = json.loads(resp.text)  # スキーマどおりのJSONが返る
//}

@<code>{response_mime_type="application/json"}と@<code>{response_schema}を組み合わせることで、モデルの出力を必ず指定したJSON形式に制約できます。これにより、安定した出力が得られます。

== UIの仕上げ

=== custom_cssで不要な要素を隠す

絵しりとりではテキスト入力を使わないため、Chainlit標準のメッセージ入力欄を非表示にします。
//emlist[public/stylesheet.css][css]{
/* テキスト入力欄を非表示にする */
#message-composer {
  display: none;
}
//}
@<code>{#message-composer}はChainlitソースコードのMessageComposer/index.tsxに由来するIDです。
@<code>{.chainlit/config.toml}に以下を追加すると適用されます。
//emlist[.chainlit/config.toml]{
[UI]
custom_css = "/public/stylesheet.css"
//}

== まとめ

本章では、ChainlitのCustomElementを活用して、チャット内にお絵かきキャンバスを埋め込み、AIと絵しりとりで遊べるWebアプリを作成しました。
Chainlitは「チャットUI」の印象が強いフレームワークですが、CustomElementを活用すれば、チャットの枠を超えたリッチなインタラクションを実現できます。
ぜひあなたも、自分だけのアプリケーションを作ってみてください！