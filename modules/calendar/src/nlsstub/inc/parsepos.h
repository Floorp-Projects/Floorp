#ifndef parsepos_h__
#define parsepos_h__

class ParsePosition
{

public:
  ParsePosition();
  ~ParsePosition();

  ParsePosition(TextOffset aIndex);
  ParsePosition(const ParsePosition& aParsePosition);

  PRBool operator==(const ParsePosition& aParsePosition) const;

};


#endif
