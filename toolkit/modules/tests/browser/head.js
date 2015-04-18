function removeDupes(list)
{
  let j = 0;
  for (let i = 1; i < list.length; i++) {
    if (list[i] != list[j]) {
      j++;
      if (i != j) {
        list[j] = list[i];
      }
    }
  }
  list.length = j + 1;
}

function compareLists(list1, list2, kind)
{
  list1.sort();
  removeDupes(list1);
  list2.sort();
  removeDupes(list2);
  is(String(list1), String(list2), `${kind} URLs correct`);
}

