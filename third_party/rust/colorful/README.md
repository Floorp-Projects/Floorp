<h1 align="center">
        <a>Colorful</a>
        <br>
        <br>
</h1>

[![Build Status](https://travis-ci.org/rocketsman/colorful.svg?branch=master)](https://travis-ci.org/rocketsman/colorful) [![Coverage Status](https://coveralls.io/repos/github/rocketsman/colorful/badge.svg?branch=master)](https://coveralls.io/github/rocketsman/colorful?branch=master) [![Codacy Badge](https://api.codacy.com/project/badge/Grade/37a45510f41445eea0168f0f07e8f7cb)](https://app.codacy.com/app/rocketsman/colorful_2?utm_source=github.com&utm_medium=referral&utm_content=rocketsman/colorful&utm_campaign=Badge_Grade_Dashboard)

## Usage

### Basic Usage

```Rust
extern crate colorful;

use colorful::Color;
use colorful::Colorful;
//use colorful::HSL;
//use colorful::RGB;

fn main() {
    let s = "Hello world";
    println!("{}", s.color(Color::Blue).bg_color(Color::Yellow).bold());
    //     println!("{}", s.color(HSL::new(1.0, 1.0, 0.5)).bold());
    //     println!("{}", s.color(RGB::new(255, 0, 0)).bold());
    println!("{}", s.blue().bg_yellow());
}
```

### Gradient

```Rust
extern crate colorful;

use colorful::Color;
use colorful::Colorful;

fn main() {
    println!("{}", "This code is editable and runnable!".gradient(Color::Red));
    println!("{}", "¡Este código es editable y ejecutable!".gradient(Color::Green));
    println!("{}", "Ce code est modifiable et exécutable !".gradient(Color::Yellow));
    println!("{}", "Questo codice è modificabile ed eseguibile!".gradient(Color::Blue));
    println!("{}", "このコードは編集して実行出来ます！".gradient(Color::Magenta));
    println!("{}", "여기에서 코드를 수정하고 실행할 수 있습니다!".gradient(Color::Cyan));
    println!("{}", "Ten kod można edytować oraz uruchomić!".gradient(Color::LightGray));
    println!("{}", "Este código é editável e executável!".gradient(Color::DarkGray));
    println!("{}", "Этот код можно отредактировать и запустить!".gradient(Color::LightRed));
    println!("{}", "Bạn có thể edit và run code trực tiếp!".gradient(Color::LightGreen));
    println!("{}", "这段代码是可以编辑并且能够运行的！".gradient(Color::LightYellow));
    println!("{}", "Dieser Code kann bearbeitet und ausgeführt werden!".gradient(Color::LightBlue));
    println!("{}", "Den här koden kan redigeras och köras!".gradient(Color::LightMagenta));
    println!("{}", "Tento kód můžete upravit a spustit".gradient(Color::LightCyan));
    println!("{}", "این کد قابلیت ویرایش و اجرا دارد!".gradient(Color::White));
    println!("{}", "โค้ดนี้สามารถแก้ไขได้และรันได้".gradient(Color::Grey0));
}

```
<div align="center">
    <img src="images/1.png" width="600px"</img>
</div>

### Gradient with style

```Rust
extern crate colorful;

use colorful::Colorful;

fn main() {
    println!("{}", "言葉にできず　凍えたままで 人前ではやさしく生きていた しわよせで　こんなふうに雑に 雨の夜にきみを　抱きしめてた".gradient_with_color(HSL::new(0.0, 1.0, 0.5), HSL::new(0.833, 1.0, 0.5)).underlined());
}
```

<div align="center">
    <img src="images/2.png" width="700px"</img>
</div>

### Bar chart

```Rust
extern crate colorful;

use colorful::Colorful;
use colorful::HSL;

fn main() {
    let s = "█";
    println!("{}\n", "Most Loved, Dreaded, and Wanted Languages".red());
    let values = vec![78.9, 75.1, 68.0, 67.0, 65.6, 65.1, 61.9, 60.4];
    let languages = vec!["Rust", "Kotlin", "Python", "TypeScript", "Go", "Swift", "JavaScript", "C#"];
    let c = languages.iter().max_by_key(|x| x.len()).unwrap();

    for (i, value) in values.iter().enumerate() {
        let h = (*value as f32 * 15.0 % 360.0) / 360.0;
        let length = (value - 30.0) as usize;
        println!("{:<width$} | {} {}%\n", languages.get(i).unwrap(), s.repeat(length).gradient(HSL::new(h, 1.0, 0.5)), value, width = c.len());
    }
}
```
Output

<div align="center">
    <img src="images/3.png" width="500px"</img>
</div>

### Animation

#### Rainbow

```Rust
extern crate colorful;

use colorful::Colorful;

fn main() {
    let text = format!("{:^50}\n{}\r\n{}", "岳飞 小重山", "昨夜寒蛩不住鸣 惊回千里梦 已三更 起身独自绕阶行 人悄悄 帘外月胧明",
                       "白首为功名 旧山松竹老 阻归程 欲将心事付瑶琴 知音少 弦断有谁听");
    text.rainbow();
}
```
Output

<div align="center">
    <img src="images/11.gif"</img>
</div>

#### Neon

```Rust
extern crate colorful;

use colorful::Colorful;

fn main() {
    let text = format!("{:^28}\n{}", "WARNING", "BIG BROTHER IS WATCHING YOU!!!");
    text.neon(RGB::new(226, 14, 14), RGB::new(158, 158, 158));
    // or you can use text.warn();
}

```
Output

<div align="center">
    <img src="images/22.gif"</img>
</div>


## Terminals compatibility

<table style="font-size: 60%; padding: 1px;">
<thead>
<tr>
<th rowspan=2>Terminal</th>
<th colspan=6 align=center>Formatting</th>
<th colspan=4 align=center>Color</th>
</tr>
<tr>
<!-- Formatting ---------><th>Bold</th><th>Dim</th><th>Underlined</th><th>Blink</th><th>Invert</th><th>Hidden</th>
<!--Color -------><th>8</th><th>16</th><th>88</th><th>256</th>
</tr>
<tbody>
<tr align=center><td>aTerm </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> ~ </td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td></tr>
<tr align=center><td>Eterm </td><td> ~ </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> ~ </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>GNOME Terminal </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>Guake </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>Konsole </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>Nautilus Terminal </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>rxvt </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> ~ </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td></tr>
<tr align=center><td>Terminator </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>Tilda </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td></tr>
<tr align=center><td>XFCE4 Terminal </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
<tr align=center><td>XTerm </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td></tr>
<tr align=center><td>xvt </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td></tr>
<tr align=center><td>Linux TTY </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/> </td><td> ~ </td><td><img src="images/no.png" alt=""/></td><td><img src="images/no.png" alt=""/></td></tr>
<tr align=center><td>VTE Terminal </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td> <img src="images/yes.png" alt=""/> </td><td><img src="images/no.png" alt=""/></td><td> <img src="images/yes.png" alt=""/></td></tr>
</tbody>
</thead>
</table>

~: Supported in a special way by the terminal.

## Todo

-   [x] Basic 16 color
-   [ ] Extra 240 color
-   [x] HSL support
-   [x] RGB support
-   [x] Gradient mode
-   [x] Rainbow mode
-   [x] Animation mode
-   [ ] Document
-   [x] Terminals compatibility

## License

[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2Fda2018%2Fcolorful.svg?type=large)](https://app.fossa.io/projects/git%2Bgithub.com%2Fda2018%2Fcolorful?ref=badge_large)
