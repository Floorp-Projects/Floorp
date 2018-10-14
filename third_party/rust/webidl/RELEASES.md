# 0.8.0

 - Update `lalrpop` to `0.16.0` to reduce package size.

# 0.7.0

 - Fixed bug where integer literals outside the range of `i64` could not be represented ([#15](https://github.com/sgodwincs/webidl-rs/pull/15) 

# 0.6.0

 - Fixed bug where the generated parser would still be in the source directory ([#13](https://github.com/sgodwincs/webidl-rs/pull/13))
 - Add back support for parsing `implements` statement for backwards compatibility with older WebIDLs
 - Add back support for parsing `legacycaller` in special operations for backwards compatibility with older WebIDLs
 - Update `lalrpop` to `0.15.1`. Version `0.15.0` cannot be used as it breaks with usage of `include!`
 - Remove unnecessary `Parser` struct, since it does not do anything. Instead of doing

 ```
 let parser = Parser::new();
 let result = parser.parse_string(...);
 ```

 You can now simply do

 ```
 let result = parse_string(...);
 ```

# 0.5.0

 - Reduced package size by excluding `mozilla_webidls.zip` and upgrading `lalrpop` to `0.14.0` ([#12](https://github.com/sgodwincs/webidl-rs/pull/12))
