/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

$(document).ready(function() {

	reset_sort_headers();

	split_debug_onto_two_rows();

	$('.col-links a.screenshot').click(function(event) {
		window.open($(this).parents('.results-table-row').next('.debug').find('.screenshot img').attr('src'));
		event.preventDefault();
	});

	$('.screenshot a').click(function(event) {
		window.open($(this).find('img').attr('src'));
		event.preventDefault();
	});

	$('.sortable').click(toggle_sort_states);

	$('.sortable').click(function() {
		var columnName = $(this).attr('col');
		if ($(this).hasClass('numeric')) {
			sort_rows_num($(this), 'col-' + columnName);
		} else {
		sort_rows_alpha($(this), 'col-' + columnName);
		}
	});

});

function sort_rows_alpha(clicked, sortclass) {
	one_row_for_data();
	var therows = $('.results-table-row');
	therows.sort(function(s, t) {
		var a = s.getElementsByClassName(sortclass)[0].innerHTML.toLowerCase();
		var b = t.getElementsByClassName(sortclass)[0].innerHTML.toLowerCase();
		if (clicked.hasClass('asc')) {
			if (a < b)
				return -1;
			if (a > b)
				return 1;
			return 0;
		} else {
			if (a < b)
				return 1;
			if (a > b)
				return -1;
			return 0;
		}
	});
	$('#results-table-body').append(therows);
	split_debug_onto_two_rows();
}

function sort_rows_num(clicked, sortclass) {
	one_row_for_data();
	var therows = $('.results-table-row');
	therows.sort(function(s, t) {
		var a = s.getElementsByClassName(sortclass)[0].innerHTML
		var b = t.getElementsByClassName(sortclass)[0].innerHTML
		if (clicked.hasClass('asc')) {
			return a - b;
		} else {
			return b - a;
		}
	});
	$('#results-table-body').append(therows);
	split_debug_onto_two_rows();
}

function reset_sort_headers() {
	$('.sort-icon').remove();
	$('.sortable').prepend('<div class="sort-icon">vvv</div>');
	$('.sortable').removeClass('asc desc inactive active');
	$('.sortable').addClass('asc inactive');
}

function toggle_sort_states() {
	//if active, toggle between asc and desc
	if ($(this).hasClass('active')) {
		$(this).toggleClass('asc');
		$(this).toggleClass('desc');
	}

	//if inactive, reset all other functions and add ascending active
	if ($(this).hasClass('inactive')) {
		reset_sort_headers();
		$(this).removeClass('inactive');
		$(this).addClass('active');
	}
}

function split_debug_onto_two_rows() {
	$('tr.results-table-row').each(function() {
		$('<tr class="debug">').insertAfter(this).append($('.debug', this));
	});
	$('td.debug').attr('colspan', 5);
}

function one_row_for_data() {
	$('tr.results-table-row').each(function() {
		if ($(this).next().hasClass('debug')) {
			$(this).append($(this).next().contents().unwrap());
		}
	});
}
