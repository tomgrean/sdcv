$(document).ready(function() {
	var flag = false;
	var dict_content = $("#dict_content");
	var qword = $("#qwt");
	var formobj = $("form");
	qword.autocomplete({
		autoFocus:true,
		source:function(req, res) {
			$.ajax({
				url:"neigh",
				type:"GET",
				dataType:"text",
				data:{"w":req.term,"off":"0","len":"10"},
				success:function(data) {
					res(data.split("\n"));
				},
				error:function() {
					res(["ERROR"]);
				}
			});
		},
		close:function(e,ui) {
			if (flag)
				formobj.submit();
			flag = false;
		},
		select:function(e,ui) {
			flag = true;
		}
	});
	formobj.on("submit", function(e) {
		e.preventDefault();
		console.log("query for word:" + qword.val());
		$.ajax({
			url:".",
			type:"GET",
			dataType:"html",
			data:{"w":qword.val(),"co":"c"},
			success:function(data) {
				dict_content.html(data);
			},
			error:function(d,txt) {
				dict_content.text(txt);
			}
		});
		return false;
	});
});
