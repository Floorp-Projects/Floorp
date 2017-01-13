// From https://github.com/tailhook/quick-error
// Changes:
//   - replace `impl Error` by `impl Item::description`
//   - $imeta

#[macro_export]
macro_rules! quick_error {
    (   $(#[$meta:meta])*
        pub enum $name:ident { $($chunks:tt)* }
    ) => {
        quick_error!(SORT [pub enum $name $(#[$meta])* ]
            items [] buf []
            queue [ $($chunks)* ]);
    };
    (   $(#[$meta:meta])*
        enum $name:ident { $($chunks:tt)* }
    ) => {
        quick_error!(SORT [enum $name $(#[$meta])* ]
            items [] buf []
            queue [ $($chunks)* ]);
    };
    // Queue is empty, can do the work
    (SORT [enum $name:ident $( #[$meta:meta] )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [ ]
        queue [ ]
    ) => {
        quick_error!(ENUM_DEFINITION [enum $name $( #[$meta] )*]
            body []
            queue [$($( #[$imeta] )*
                      => $iitem: $imode [$( $ivar: $ityp ),*] )*]
        );
        quick_error!(IMPLEMENTATIONS $name {$(
           $iitem: $imode [$(#[$imeta])*] [$( $ivar: $ityp ),*] {$( $ifuncs )*}
           )*});
        $(
            quick_error!(ERROR_CHECK $imode $($ifuncs)*);
        )*
    };
    (SORT [pub enum $name:ident $( #[$meta:meta] )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [ ]
        queue [ ]
    ) => {
        quick_error!(ENUM_DEFINITION [pub enum $name $( #[$meta] )*]
            body []
            queue [$($( #[$imeta] )*
                      => $iitem: $imode [$( $ivar: $ityp ),*] )*]
        );
        quick_error!(IMPLEMENTATIONS $name {$(
           $iitem: $imode [$(#[$imeta])*] [$( $ivar: $ityp ),*] {$( $ifuncs )*}
           )*});
        $(
            quick_error!(ERROR_CHECK $imode $($ifuncs)*);
        )*
    };
    // Add meta to buffer
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )*]
        queue [ #[$qmeta:meta] $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*]
            buf [$( #[$bmeta] )* #[$qmeta] ]
            queue [$( $tail )*]);
    };
    // Add ident to buffer
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )*]
        queue [ $qitem:ident $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])*
                      => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*]
            buf [$(#[$bmeta])* => $qitem : UNIT [ ] ]
            queue [$( $tail )*]);
    };
    // Flush buffer on meta after ident
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )*
            => $bitem:ident: $bmode:tt [$( $bvar:ident: $btyp:ty ),*] ]
        queue [ #[$qmeta:meta] $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            enum [$( $(#[$emeta])* => $eitem $(( $($etyp),* ))* )*
                     $(#[$bmeta])* => $bitem: $bmode $(( $($btyp),* ))*]
            items [$($( #[$imeta:meta] )*
                      => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*
                     $bitem: $bmode [$( $bvar:$btyp ),*] {} ]
            buf [ #[$qmeta] ]
            queue [$( $tail )*]);
    };
    // Add tuple enum-variant
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )* => $bitem:ident: UNIT [ ] ]
        queue [($( $qvar:ident: $qtyp:ty ),+) $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*]
            buf [$( #[$bmeta] )* => $bitem: TUPLE [$( $qvar:$qtyp ),*] ]
            queue [$( $tail )*]
        );
    };
    // Add struct enum-variant - e.g. { descr: &'static str }
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )* => $bitem:ident: UNIT [ ] ]
        queue [{ $( $qvar:ident: $qtyp:ty ),+} $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*]
            buf [$( #[$bmeta] )* => $bitem: STRUCT [$( $qvar:$qtyp ),*] ]
            queue [$( $tail )*]);
    };
    // Add struct enum-variant, with excess comma - e.g. { descr: &'static str, }
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )* => $bitem:ident: UNIT [ ] ]
        queue [{$( $qvar:ident: $qtyp:ty ),+ ,} $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*]
            buf [$( #[$bmeta] )* => $bitem: STRUCT [$( $qvar:$qtyp ),*] ]
            queue [$( $tail )*]);
    };
    // Add braces and flush always on braces
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )*
                 => $bitem:ident: $bmode:tt [$( $bvar:ident: $btyp:ty ),*] ]
        queue [ {$( $qfuncs:tt )*} $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*
                      $(#[$bmeta])* => $bitem: $bmode [$( $bvar:$btyp ),*] {$( $qfuncs )*} ]
            buf [ ]
            queue [$( $tail )*]);
    };
    // Flush buffer on double ident
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )*
                 => $bitem:ident: $bmode:tt [$( $bvar:ident: $btyp:ty ),*] ]
        queue [ $qitem:ident $( $tail:tt )*]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*
                     $(#[$bmeta])* => $bitem: $bmode [$( $bvar:$btyp ),*] {} ]
            buf [ => $qitem : UNIT [ ] ]
            queue [$( $tail )*]);
    };
    // Flush buffer on end
    (SORT [$( $def:tt )*]
        items [$($( #[$imeta:meta] )*
                  => $iitem:ident: $imode:tt [$( $ivar:ident: $ityp:ty ),*]
                                {$( $ifuncs:tt )*} )* ]
        buf [$( #[$bmeta:meta] )*
            => $bitem:ident: $bmode:tt [$( $bvar:ident: $btyp:ty ),*] ]
        queue [ ]
    ) => {
        quick_error!(SORT [$( $def )*]
            items [$( $(#[$imeta])* => $iitem: $imode [$( $ivar:$ityp ),*] {$( $ifuncs )*} )*
                     $(#[$bmeta])* => $bitem: $bmode [$( $bvar:$btyp ),*] {} ]
            buf [ ]
            queue [ ]);
    };
    // Public enum (Queue Empty)
    (ENUM_DEFINITION [pub enum $name:ident $( #[$meta:meta] )*]
        body [$($( #[$imeta:meta] )*
            => $iitem:ident ($(($( $ttyp:ty ),+))*) {$({$( $svar:ident: $styp:ty ),*})*} )* ]
        queue [ ]
    ) => {
        $(#[$meta])*
        pub enum $name {
            $(
                $(#[$imeta])*
                $iitem $(($( $ttyp ),*))* $({$( $svar: $styp ),*})*,
            )*
        }
    };
    // Private enum (Queue Empty)
    (ENUM_DEFINITION [enum $name:ident $( #[$meta:meta] )*]
        body [$($( #[$imeta:meta] )*
            => $iitem:ident ($(($( $ttyp:ty ),+))*) {$({$( $svar:ident: $styp:ty ),*})*} )* ]
        queue [ ]
    ) => {
        $(#[$meta])*
        enum $name {
            $(
                $(#[$imeta])*
                $iitem $(($( $ttyp ),*))* $({$( $svar: $styp ),*})*,
            )*
        }
    };
    // Unit variant
    (ENUM_DEFINITION [$( $def:tt )*]
        body [$($( #[$imeta:meta] )*
            => $iitem:ident ($(($( $ttyp:ty ),+))*) {$({$( $svar:ident: $styp:ty ),*})*} )* ]
        queue [$( #[$qmeta:meta] )*
            => $qitem:ident: UNIT [ ] $( $queue:tt )*]
    ) => {
        quick_error!(ENUM_DEFINITION [ $($def)* ]
            body [$($( #[$imeta] )* => $iitem ($(($( $ttyp ),+))*) {$({$( $svar: $styp ),*})*} )*
                    $( #[$qmeta] )* => $qitem () {} ]
            queue [ $($queue)* ]
        );
    };
    // Tuple variant
    (ENUM_DEFINITION [$( $def:tt )*]
        body [$($( #[$imeta:meta] )*
            => $iitem:ident ($(($( $ttyp:ty ),+))*) {$({$( $svar:ident: $styp:ty ),*})*} )* ]
        queue [$( #[$qmeta:meta] )*
            => $qitem:ident: TUPLE [$( $qvar:ident: $qtyp:ty ),+] $( $queue:tt )*]
    ) => {
        quick_error!(ENUM_DEFINITION [ $($def)* ]
            body [$($( #[$imeta] )* => $iitem ($(($( $ttyp ),+))*) {$({$( $svar: $styp ),*})*} )*
                    $( #[$qmeta] )* => $qitem (($( $qtyp ),*)) {} ]
            queue [ $($queue)* ]
        );
    };
    // Struct variant
    (ENUM_DEFINITION [$( $def:tt )*]
        body [$($( #[$imeta:meta] )*
            => $iitem:ident ($(($( $ttyp:ty ),+))*) {$({$( $svar:ident: $styp:ty ),*})*} )* ]
        queue [$( #[$qmeta:meta] )*
            => $qitem:ident: STRUCT [$( $qvar:ident: $qtyp:ty ),*] $( $queue:tt )*]
    ) => {
        quick_error!(ENUM_DEFINITION [ $($def)* ]
            body [$($( #[$imeta] )* => $iitem ($(($( $ttyp ),+))*) {$({$( $svar: $styp ),*})*} )*
                    $( #[$qmeta] )* => $qitem () {{$( $qvar: $qtyp ),*}} ]
            queue [ $($queue)* ]
        );
    };
    (IMPLEMENTATIONS
        $name:ident {$(
            $item:ident: $imode:tt [$(#[$imeta:meta])*] [$( $var:ident: $typ:ty ),*] {$( $funcs:tt )*}
        )*}
    ) => {
        #[allow(unused)]
        impl ::std::fmt::Display for $name {
            fn fmt(&self, fmt: &mut ::std::fmt::Formatter)
                -> ::std::fmt::Result
            {
                match *self {
                    $(
                        $(#[$imeta])*
                        quick_error!(ITEM_PATTERN
                            $name $item: $imode [$( ref $var ),*]
                        ) => {
                            let display_fn = quick_error!(FIND_DISPLAY_IMPL
                                $name $item: $imode
                                {$( $funcs )*});

                            display_fn(self, fmt)
                        }
                    )*
                }
            }
        }
        /*#[allow(unused)]
        impl ::std::error::Error for $name {
            fn description(&self) -> &str {
                match *self {
                    $(
                        quick_error!(ITEM_PATTERN
                            $name $item: $imode [$( ref $var ),*]
                        ) => {
                            quick_error!(FIND_DESCRIPTION_IMPL
                                $item: $imode self fmt [$( $var ),*]
                                {$( $funcs )*})
                        }
                    )*
                }
            }
            fn cause(&self) -> Option<&::std::error::Error> {
                match *self {
                    $(
                        quick_error!(ITEM_PATTERN
                            $name $item: $imode [$( ref $var ),*]
                        ) => {
                            quick_error!(FIND_CAUSE_IMPL
                                $item: $imode [$( $var ),*]
                                {$( $funcs )*})
                        }
                    )*
                }
            }
        }*/
        #[allow(unused)]
        impl $name {
            /// A string describing the error kind.
            pub fn description(&self) -> &str {
                match *self {
                    $(
                        $(#[$imeta])*
                        quick_error!(ITEM_PATTERN
                            $name $item: $imode [$( ref $var ),*]
                        ) => {
                            quick_error!(FIND_DESCRIPTION_IMPL
                                $item: $imode self fmt [$( $var ),*]
                                {$( $funcs )*})
                        }
                    )*
                }
            }
        }
        $(
            quick_error!(FIND_FROM_IMPL
                $name $item: $imode [$( $var:$typ ),*]
                {$( $funcs )*});
        )*
    };
    (FIND_DISPLAY_IMPL $name:ident $item:ident: $imode:tt
        { display($self_:tt) -> ($( $exprs:tt )*) $( $tail:tt )*}
    ) => {
        |quick_error!(IDENT $self_): &$name, f: &mut ::std::fmt::Formatter| {
            write!(f, $( $exprs )*)
        }
    };
    (FIND_DISPLAY_IMPL $name:ident $item:ident: $imode:tt
        { display($pattern:expr) $( $tail:tt )*}
    ) => {
        |_, f: &mut ::std::fmt::Formatter| { write!(f, $pattern) }
    };
    (FIND_DISPLAY_IMPL $name:ident $item:ident: $imode:tt
        { display($pattern:expr, $( $exprs:tt )*) $( $tail:tt )*}
    ) => {
        |_, f: &mut ::std::fmt::Formatter| { write!(f, $pattern, $( $exprs )*) }
    };
    (FIND_DISPLAY_IMPL $name:ident $item:ident: $imode:tt
        { $t:tt $( $tail:tt )*}
    ) => {
        quick_error!(FIND_DISPLAY_IMPL
            $name $item: $imode
            {$( $tail )*})
    };
    (FIND_DISPLAY_IMPL $name:ident $item:ident: $imode:tt
        { }
    ) => {
        |self_: &$name, f: &mut ::std::fmt::Formatter| {
            write!(f, "{}", self_.description())
        }
    };
    (FIND_DESCRIPTION_IMPL $item:ident: $imode:tt $me:ident $fmt:ident
        [$( $var:ident ),*]
        { description($expr:expr) $( $tail:tt )*}
    ) => {
        $expr
    };
    (FIND_DESCRIPTION_IMPL $item:ident: $imode:tt $me:ident $fmt:ident
        [$( $var:ident ),*]
        { $t:tt $( $tail:tt )*}
    ) => {
        quick_error!(FIND_DESCRIPTION_IMPL
            $item: $imode $me $fmt [$( $var ),*]
            {$( $tail )*})
    };
    (FIND_DESCRIPTION_IMPL $item:ident: $imode:tt $me:ident $fmt:ident
        [$( $var:ident ),*]
        { }
    ) => {
        stringify!($item)
    };
    (FIND_CAUSE_IMPL $item:ident: $imode:tt
        [$( $var:ident ),*]
        { cause($expr:expr) $( $tail:tt )*}
    ) => {
        Some($expr)
    };
    (FIND_CAUSE_IMPL $item:ident: $imode:tt
        [$( $var:ident ),*]
        { $t:tt $( $tail:tt )*}
    ) => {
        quick_error!(FIND_CAUSE_IMPL
            $item: $imode [$( $var ),*]
            { $($tail)* })
    };
    (FIND_CAUSE_IMPL $item:ident: $imode:tt
        [$( $var:ident ),*]
        { }
    ) => {
        None
    };
    (FIND_FROM_IMPL $name:ident $item:ident: $imode:tt
        [$( $var:ident: $typ:ty ),*]
        { from() $( $tail:tt )*}
    ) => {
        $(
            impl From<$typ> for $name {
                fn from($var: $typ) -> $name {
                    $name::$item($var)
                }
            }
        )*
        quick_error!(FIND_FROM_IMPL
            $name $item: $imode [$( $var:$typ ),*]
            {$( $tail )*});
    };
    (FIND_FROM_IMPL $name:ident $item:ident: UNIT
        [ ]
        { from($ftyp:ty) $( $tail:tt )*}
    ) => {
        impl From<$ftyp> for $name {
            fn from(_discarded_error: $ftyp) -> $name {
                $name::$item
            }
        }
        quick_error!(FIND_FROM_IMPL
            $name $item: UNIT [  ]
            {$( $tail )*});
    };
    (FIND_FROM_IMPL $name:ident $item:ident: TUPLE
        [$( $var:ident: $typ:ty ),*]
        { from($fvar:ident: $ftyp:ty) -> ($( $texpr:expr ),*) $( $tail:tt )*}
    ) => {
        impl From<$ftyp> for $name {
            fn from($fvar: $ftyp) -> $name {
                $name::$item($( $texpr ),*)
            }
        }
        quick_error!(FIND_FROM_IMPL
            $name $item: TUPLE [$( $var:$typ ),*]
            { $($tail)* });
    };
    (FIND_FROM_IMPL $name:ident $item:ident: STRUCT
        [$( $var:ident: $typ:ty ),*]
        { from($fvar:ident: $ftyp:ty) -> {$( $tvar:ident: $texpr:expr ),*} $( $tail:tt )*}
    ) => {
        impl From<$ftyp> for $name {
            fn from($fvar: $ftyp) -> $name {
                $name::$item {
                    $( $tvar: $texpr ),*
                }
            }
        }
        quick_error!(FIND_FROM_IMPL
            $name $item: STRUCT [$( $var:$typ ),*]
            { $($tail)* });
    };
    (FIND_FROM_IMPL $name:ident $item:ident: $imode:tt
        [$( $var:ident: $typ:ty ),*]
        { $t:tt $( $tail:tt )*}
    ) => {
        quick_error!(FIND_FROM_IMPL
            $name $item: $imode [$( $var:$typ ),*]
            {$( $tail )*}
        );
    };
    (FIND_FROM_IMPL $name:ident $item:ident: $imode:tt
        [$( $var:ident: $typ:ty ),*]
        { }
    ) => {
    };
    (ITEM_BODY $(#[$imeta:meta])* $item:ident: UNIT
    ) => { };
    (ITEM_BODY $(#[$imeta:meta])* $item:ident: TUPLE
        [$( $typ:ty ),*]
    ) => {
        ($( $typ ),*)
    };
    (ITEM_BODY $(#[$imeta:meta])* $item:ident: STRUCT
        [$( $var:ident: $typ:ty ),*]
    ) => {
        {$( $var:$typ ),*}
    };
    (ITEM_PATTERN $name:ident $item:ident: UNIT []
    ) => {
        $name::$item
    };
    (ITEM_PATTERN $name:ident $item:ident: TUPLE
        [$( ref $var:ident ),*]
    ) => {
        $name::$item ($( ref $var ),*)
    };
    (ITEM_PATTERN $name:ident $item:ident: STRUCT
        [$( ref $var:ident ),*]
    ) => {
        $name::$item {$( ref $var ),*}
    };
    // This one should match all allowed sequences in "funcs" but not match
    // anything else.
    // This is to contrast FIND_* clauses which just find stuff they need and
    // skip everything else completely
    (ERROR_CHECK $imode:tt display($self_:tt) -> ($( $exprs:tt )*) $( $tail:tt )*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK $imode:tt display($pattern: expr) $( $tail:tt )*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK $imode:tt display($pattern: expr, $( $exprs:tt )*) $( $tail:tt )*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK $imode:tt description($expr:expr) $( $tail:tt )*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK $imode:tt cause($expr:expr) $($tail:tt)*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK $imode:tt from() $($tail:tt)*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK $imode:tt from($ftyp:ty) $($tail:tt)*)
    => { quick_error!(ERROR_CHECK $imode $($tail)*); };
    (ERROR_CHECK TUPLE from($fvar:ident: $ftyp:ty) -> ($( $e:expr ),*) $( $tail:tt )*)
    => { quick_error!(ERROR_CHECK TUPLE $($tail)*); };
    (ERROR_CHECK STRUCT from($fvar:ident: $ftyp:ty) -> {$( $v:ident: $e:expr ),*} $( $tail:tt )*)
    => { quick_error!(ERROR_CHECK STRUCT $($tail)*); };
    (ERROR_CHECK $imode:tt ) => {};
    // Utility functions
    (IDENT $ident:ident) => { $ident }
}
