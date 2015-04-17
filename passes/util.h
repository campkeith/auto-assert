#define each_custom(item, set, begin, end) (__typeof((set).begin()) item = (set).begin(), __end = (set).end(); item != __end; ++item)
#define each(item, set) each_custom(item, set, begin, end)
