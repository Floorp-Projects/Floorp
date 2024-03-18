# Android Accessibility Guide
Accessibility (or "a11y") is about making apps usable by everyone. Android developers need to pay particular attention to users with visual impairment, auditory impairment, and restricted motor skills. This guide is intended to get you started with making your Android app more accessible.

To identify how well the accessibility of your app is working, try it as a user! For example, try closing your eyes while using the TalkBack screen reader. You can also use [the Accessibility Scanner][scanner] for an automated analysis.

If you have questions, please ask!

## Introductory resources
The [official Android accessibility overview](https://developer.android.com/guide/topics/ui/accessibility/apps) is a great overview to common issues users may face and how you should address them as an Android developer. A good supplement, for visual impairment, is [this official Android Developers video](https://www.youtube.com/watch?v=1by5J7c5Vz4). This [presentation on accessibility](https://www.youtube.com/watch?v=UOr3mgqJU0A&feature=youtu.be&list=PLnVy79PaFHMUqqvwbjyKJZv1N8rzHOCBi) and [this podcast](https://androidbackstage.blogspot.com/2014/10/episode-14-accessibility.html) are also recommended by our developers.

Some general tips:
- Look through the accessibility preferences in the Developer Options: there are many features, such as visual simulations, to help you.
- If it is impossible to provide an intuitive, accessible user experience for your current UI, this is a design smell: a UI that is accessible tends to be the best experience for all users.
    - For example, a dialog with one option, like "Remove", that is dismissed by clicking outside the dialog, will be unintuitive to screen reader users. However, this dialog may also be unintuitive to users who are unfamiliar with common software paradigms (i.e. tap outside to dismiss) too. A better dialog would be one with a "dismiss" option in addition to "remove"
- Avoid creating separate code paths and user experiences to address accessibility issues: in our experience, accessibility is not often tested so it's easy to break the experience if you forget to update one code path

## Screen reader tips
**TalkBack** is the default Android system screen reader: it's usually installed by default but you may need to install it from the Play Store. When testing, don't forget your headphones!

**VoiceView** is the default system screen reader on Amazon products: it is installed by default.

Some development tips:
- Don't try to override TalkBack's behavior outside of what is suggested in the docs: users expect the system to work consistently so overriding that behavior can be unintuitive.
    - Generally let the system handle focus changes: it's unintuitive when it jumps around.
- Overriding the accessibility APIs in code is a code smell: generally, you can make the changes you need using the methods recommended in the docs, which provides a more maintainable and intuitive experience to users. Also note the code APIs are unintuitive and not well documented.

### Cut out redundant steps with `android:importantForAccessibility="no"`
Unintuitively, setting some views to not be important for accessibility improves accessibility.
For example, take this layout that represents a menu item that consists of an image and text.
```
<LinearLayout>
  <ImageView/>
  <TextView/>
</LinearLayout>
```
When a user swipes right past this composite view, the cursor will first land on the LinearLayout, and then on the TextView:
<img alt="Animation of cursor landing menu items and their text children" src="https://github.com/mozilla-mobile/firefox-android/raw/main/docs/shared/android/images/double-nav.gif" width="240">

This is both inefficient and confusing for the user, since they will encounter each item twice. To streamline this experience, set `android:importantForAccessibility="no"` on the `TextView`. This will tell TalkBack not to bother with that node:
<img alt="Animation of cursor only landing on menu items" src="https://github.com/mozilla-mobile/firefox-android/raw/main/docs/shared/android/images/single-nav.gif" width="240">

## Automated testing
You can regularly use [the Accessibility Scanner][scanner] to get a cursory overview of how the accessibility of your app is doing.

When adding specialized accessibility logic, it is encouraged to add as much testing as possible since these code paths are not exercised in typical use and the chance of regression is high. Below is an example Mockito and Robolectric test that validates accessibility events that are resulted from a [loading progress update](https://github.com/mozilla-mobile/android-components/pull/2526).

```kotlin
@Test
fun `displayProgress will send accessibility events`() {
    val toolbar = BrowserToolbar(context)
    val root = mock(ViewParent::class.java)
    Shadows.shadowOf(toolbar).setMyParent(root)
    `when`(root.requestSendAccessibilityEvent(any(), any())).thenReturn(false)

    Shadows.shadowOf(context.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager).setEnabled(true)

    toolbar.displayProgress(10)
    toolbar.displayProgress(50)
    toolbar.displayProgress(100)

    val captor = ArgumentCaptor.forClass(AccessibilityEvent::class.java)

    verify(root, times(4)).requestSendAccessibilityEvent(any(), captor.capture())

    assertEquals(AccessibilityEvent.TYPE_ANNOUNCEMENT, captor.allValues[0].eventType)
    assertEquals(context.getString(R.string.mozac_browser_toolbar_progress_loading), captor.allValues[0].text[0])

    assertEquals(AccessibilityEvent.TYPE_VIEW_SCROLLED, captor.allValues[1].eventType)
    assertEquals(10, captor.allValues[1].scrollY)
    assertEquals(100, captor.allValues[1].maxScrollY)

    assertEquals(AccessibilityEvent.TYPE_VIEW_SCROLLED, captor.allValues[2].eventType)
    assertEquals(50, captor.allValues[2].scrollY)
    assertEquals(100, captor.allValues[2].maxScrollY)

    assertEquals(AccessibilityEvent.TYPE_VIEW_SCROLLED, captor.allValues[3].eventType)
    assertEquals(100, captor.allValues[3].scrollY)
    assertEquals(100, captor.allValues[3].maxScrollY)
}
```

In addition the [official documentation](https://developer.android.com/training/accessibility/testing#automated) seems like a good starting point to learn more about accessibility validation in tests. We should look into integrating this into our CI.

[scanner]: https://support.google.com/accessibility/android/answer/6376570?hl=en
