if (typeof(dojo) != 'undefined') { dojo.require('MochiKit.Signal'); }
if (typeof(JSAN) != 'undefined') { JSAN.use('MochiKit.Signal'); }
if (typeof(tests) == 'undefined') { tests = {}; }

tests.test_DragAndDrop = function (t) {
    
    var drag1 = new MochiKit.DragAndDrop.Draggable('drag1', {'revert': true, 'ghosting': true});

    var drop1 = new MochiKit.DragAndDrop.Droppable('drop1', {'hoverclass': 'drop-hover'});
    drop1.activate();
    t.is(hasElementClass('drop1', 'drop-hover'), true, "hoverclass ok");
    drop1.deactivate();
    t.is(hasElementClass('drop1', 'drop-hover'), false, "remove hoverclass ok");
    drop1.destroy();
    
    t.is( isEmpty(MochiKit.DragAndDrop.Droppables.drops), true, "Unregister droppable ok");
    
    var onhover = function (element) {
        t.is(element, getElement('drag1'), 'onhover ok');
    };
    var drop2 = new MochiKit.DragAndDrop.Droppable('drop1', {'onhover': onhover});
    var pos = getElementPosition('drop1');
    pos = {"x": pos.x + 5, "y": pos.y + 5};
    MochiKit.DragAndDrop.Droppables.show({"page": pos}, getElement('drag1'));

    drag1.destroy();
    t.is( isEmpty(MochiKit.DragAndDrop.Draggables.drops), true, "Unregister draggable ok");
    
};

