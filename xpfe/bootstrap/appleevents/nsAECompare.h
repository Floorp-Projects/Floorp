/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */



class AEComparisons
{
public:

	static Boolean	CompareTexts(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean	CompareEnumeration(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean 	CompareInteger(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean 	CompareFixed(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean 	CompareFloat(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean 	CompareBoolean(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean 	CompareRGBColor(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean	ComparePoint(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	static Boolean 	CompareRect(DescType oper, const AEDesc *desc1, const AEDesc *desc2);
	
	static Boolean 	TryPrimitiveComparison(DescType comparisonOperator, const AEDesc *desc1, const AEDesc *desc2);
	
protected:
	static Boolean EqualRGB(RGBColor colorA, RGBColor colorB)
	{
		return((colorA.red == colorB.red) && (colorA.green == colorB.green) && (colorA.blue == colorB.blue));
	}
};

